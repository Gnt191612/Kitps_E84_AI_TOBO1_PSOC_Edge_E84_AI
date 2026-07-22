/**
 * @file system.c
 * @brief 系统初始化与调度实现
 *
 * ESP32: Core 0 跑协议/UART, Core 1 跑跟踪算法
 *
 * ESP32-A (CONFIG_ESP32_IS_SERVER=y): WS 服务器 + 连 H7
 * ESP32-B (!CONFIG_ESP32_IS_SERVER):  WS 客户端连 A
 *
 * 浏览器指令: TRACK/RELEASE/RELOAD/SWITCH
 * ESP32-B 数据: JSON {"s":"B","t":1,"x":...,"y":...,"l":...}
 */

#include "system.h"
#include "main.h"
#include "Communication/protocol.h"
#include "Communication/network_manager.h"
#include "Drivers/uart/uart.h"
#include "Drivers/gpio/gpio.h"
#include "Drivers/pwm/pwm.h"
#include "Drivers/ov2640/ov2640.h"
#include "Vision/capture.h"
#include "Vision/preprocess.h"
#include "tracking/tracker.h"
#include "tracking/relock.h"
#include "data_logger/logger.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

/* ──── 全局跟踪器 ──── */
static Tracker_t s_tracker;

/* ──── 帧缓冲区 ──── */
static uint8_t *s_rgb_buf = NULL;
static uint8_t *s_gray_buf = NULL;
static uint8_t *s_working_buf = NULL;

/* ──── 运行时角色标志 ──── */
static int s_is_server = 0;

/* ──── H7 文本上传回调 → 转发到 WebSocket (仅服务器模式) ──── */
static void on_h7_upload(const uint8_t *data, uint16_t len)
{
    if (s_is_server && Network_ClientCount() > 0) {
        Network_BroadcastText((const char *)data, len);
    }
}

/* ──── H7 命令回调 ──── */
/* ──── H7 angle/dist → 像素坐标转换 ──── */
/*
 * OV2640 QQVGA 160x120, 水平 FOV ≈ 50°, 竖直 FOV ≈ 38°
 * px_per_deg = 160 / 50 ≈ 3.2, center = (80, 60)
 */
#define CAM_FOV_H_DEG    50.0f
#define PX_PER_DEG       (OV2640_WIDTH / CAM_FOV_H_DEG)   /* ≈ 3.2 */
#define CAM_CENTER_X     (OV2640_WIDTH / 2)
#define CAM_CENTER_Y     (OV2640_HEIGHT / 2)

/* 将 H7 传来的 angle(°)/dist(cm) 转为预测像素坐标 */
/* 返回 -1,-1 表示超出视野，应做全图扫描 */
static void angle_dist_to_px(int32_t angle_deg, int32_t dist_cm,
                              int *px, int *py)
{
    if (dist_cm <= 0) {
        /* 浏览器传 0:0:0 表示没有预测位置 */
        *px = -1; *py = -1;
        return;
    }
    /* 角度范围 ±25° 以内 → 落在视野内 */
    if (angle_deg > -25 && angle_deg < 25) {
        *px = (int)(CAM_CENTER_X + angle_deg * PX_PER_DEG);
        *py = CAM_CENTER_Y;
        /* 限制在图像边界内 */
        if (*px < 0) { *px = 0; }
        if (*px >= OV2640_WIDTH) { *px = OV2640_WIDTH - 1; }
        if (*py < 0) { *py = 0; }
        if (*py >= OV2640_HEIGHT) { *py = OV2640_HEIGHT - 1; }
    } else {
        *px = -1; *py = -1;  /* 超出视野 → 全图扫 */
    }
}

/* ──── 执行复锁（含预测位置） ──── */
static void do_relock_with_predict(int32_t angle_deg, int32_t dist_cm)
{
    int px, py;
    angle_dist_to_px(angle_deg, dist_cm, &px, &py);

    float out_x = 0, out_y = 0;
    int ret;

    if (px >= 0 && py >= 0) {
        ret = Relock_ExecuteAt(s_tracker.frame_buf,
                               OV2640_WIDTH, OV2640_HEIGHT,
                               px, py, &out_x, &out_y);
    } else {
        ret = Relock_Execute(s_tracker.frame_buf,
                             OV2640_WIDTH, OV2640_HEIGHT,
                             &out_x, &out_y);
    }

    if (ret == 0) {
        /* 复锁成功: 更新跟踪器位置再开始跟踪 */
        s_tracker.result_x_mm = out_x;
        s_tracker.result_y_mm = out_y;
        Tracker_Start(&s_tracker);
        LOG_INFO("Relock+track OK: (%.1f, %.1f)mm", out_x, out_y);
    } else {
        /* 复锁失败: 仍然启动跟踪, 让算法自己重试 */
        Tracker_Start(&s_tracker);
        LOG_WARN("Relock+track failed, starting blind");
    }
}

static void on_h7_command(uint8_t cmd, uint8_t target_id, int32_t param1, int32_t param2)
{
    LOG_INFO("H7 cmd: 0x%02X, target=%d, param1=%ld, param2=%ld",
             cmd, target_id, (long)param1, (long)param2);

    switch (cmd) {
    case CMD_TRACK:
        LOG_INFO("CMD_TRACK: angle=%ld°, dist=%ldcm", (long)param1, (long)param2);
        s_tracker.target_id = target_id;
        do_relock_with_predict(param1, param2);
        break;
    case CMD_ASSIST_POS:
        LOG_INFO("CMD_ASSIST_POS");
        do_relock_with_predict(param1, param2);
        break;
    case CMD_RELOCK:
        LOG_INFO("CMD_RELOCK");
        Tracker_Relock(&s_tracker);
        break;
    case CMD_RELEASE:
        LOG_INFO("CMD_RELEASE: stop tracking");
        Tracker_Stop(&s_tracker);
        break;
    default:
        LOG_WARN("Unknown cmd: 0x%02X", cmd);
        break;
    }
}

/* ──── 浏览器/转发指令回调 (ESP32-A: 来自浏览器; ESP32-B: 来自 A 转发) ──── */
static void on_command(const char *cmd, const char *params)
{
    LOG_INFO("CMD: %s, params: %s", cmd, params);

    if (strcmp(cmd, "TRACK") == 0) {
        int tgt = 0, angle = 0, dist = 0;
        sscanf(params, "%d:%d:%d", &tgt, &angle, &dist);
        s_tracker.target_id = (uint8_t)tgt;
        Tracker_Start(&s_tracker);
        LOG_INFO("TRACK: target=%d, angle=%d°, dist=%dcm", tgt, angle, dist);
    }
    else if (strcmp(cmd, "RELEASE") == 0) {
        Tracker_Stop(&s_tracker);
    }
    else if (strcmp(cmd, "RELOAD") == 0) {
        Tracker_Relock(&s_tracker);
    }
    else if (strcmp(cmd, "SWITCH") == 0) {
        int tgt = 0;
        sscanf(params, "%d", &tgt);
        LOG_INFO("SWITCH: target=%d (forwarded to H7)", tgt);
        /* 不在此处切换 — 由 H7 统一调度分派 */
    }
    else {
        LOG_WARN("Unknown cmd: %s", cmd);
        return;
    }

    /* 转发指令给 H7（浏览器 → ESP32-A → UART → H7）
     * SWITCH / TRACK / RELEASE / RELOAD 均用 FRAME_TYPE_BROWSER_CMD 帧类型（0x04）
     * 载荷: [cmd_code:1B][target_id:1B] */
    uint8_t cmd_code;
    uint8_t tgt = 0;
    unsigned int tmp;
    int p1 = 0, p2 = 0;

    if (strcmp(cmd, "TRACK") == 0) {
        cmd_code = 0x02;
        sscanf(params, "%u:%d:%d", &tmp, &p1, &p2);
        tgt = (uint8_t)tmp;
    } else if (strcmp(cmd, "SWITCH") == 0) {
        cmd_code = 0x01;
        sscanf(params, "%u", &tmp);
        tgt = (uint8_t)tmp;
    } else if (strcmp(cmd, "RELEASE") == 0) {
        cmd_code = 0x03;
    } else if (strcmp(cmd, "RELOAD") == 0) {
        cmd_code = 0x04;
    } else {
        return;
    }

    uint8_t payload[3] = { cmd_code, (uint8_t)tgt, 0 };
    uint8_t buf[FRAME_OVERHEAD + 3];
    uint16_t frame_len = Frame_Pack(FRAME_TYPE_BROWSER_CMD, payload, 3, buf);
    UART_Send(UART_H7, buf, frame_len);
    LOG_INFO("Forwarded to H7: browser cmd=0x%02X, target=%d", cmd_code, tgt);
}

/* ──── UART 接收回调 ──── */
static void uart_rx_callback(uint8_t byte)
{
    Protocol_ParseByte(Protocol_GetHandle(), byte);
}

/* ──── 系统初始化 ──── */
void System_Init(void)
{
    ESP_LOGI("SYS", "System_Init starting...");

    Logger_Init();

    /* UART + H7 通信 */
    UART_Init(UART_H7, 115200, PIN_UART1_TX, PIN_UART1_RX);
    UART_SetRxCallback(UART_H7, uart_rx_callback);
    xTaskCreatePinnedToCore(UART_RxTask, "uart_rx", UART_RX_TASK_STACK,
                            (void *)(intptr_t)UART_H7, 10, NULL, 0);

    /* 协议 */
    Protocol_Init(NULL, on_h7_command);
    /* upload callback 在角色检测后按运行时注册 */

    /* 摄像头 */
    OV2640_Init(OV2640_PIXFORMAT_RGB565, NULL);
    Capture_Init();

    /* PWM 舵机 */
    PWM_Init(LEDC_CHANNEL_2, 50, GPIO_NUM_12, LEDC_TIMER_2, LEDC_LOW_SPEED_MODE);
    PWM_Init(LEDC_CHANNEL_3, 50, GPIO_NUM_14, LEDC_TIMER_3, LEDC_LOW_SPEED_MODE);

    /* 跟踪器 */
    Tracker_Init(&s_tracker, 0, OV2640_WIDTH, OV2640_HEIGHT);

    /* 帧缓冲区 */
    size_t fb = OV2640_WIDTH * OV2640_HEIGHT * 2;
    s_rgb_buf = (uint8_t *)heap_caps_malloc(fb, MALLOC_CAP_SPIRAM);
    if (!s_rgb_buf) s_rgb_buf = (uint8_t *)malloc(fb);
    s_gray_buf = (uint8_t *)malloc(OV2640_WIDTH * OV2640_HEIGHT);
    s_working_buf = (uint8_t *)malloc(OV2640_WIDTH * OV2640_HEIGHT);

    /* ── 角色检测: GPIO4 高=服务器(A), 低=客户端(B) ── */
    gpio_set_direction(GPIO_ROLE_DETECT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_ROLE_DETECT, GPIO_PULLDOWN_ONLY);
    s_is_server = gpio_get_level(GPIO_ROLE_DETECT);

    if (s_is_server) {
        ESP_LOGI("SYS", "Role: ESP32-A (WS Server + H7)");
        Protocol_RegisterUploadCallback(on_h7_upload);
        Network_Init();
        Network_RegisterCommandCallback(on_command);
    } else {
        ESP_LOGI("SYS", "Role: ESP32-B (WS Client)");
        Network_InitClient();
        Network_RegisterRelayCallback(on_command);
    }

    LOG_INFO("System init complete. %dx%d",
             OV2640_WIDTH, OV2640_HEIGHT);
}

/* ──── 调度器 ──── */
void Scheduler_Run(void)
{
    if (!s_rgb_buf || !s_gray_buf || !s_working_buf) {
        LOG_ERROR("Buffers not allocated!");
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    int ret = Capture_Frame(s_rgb_buf, OV2640_WIDTH, OV2640_HEIGHT);
    if (ret != 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }

    Preprocess_ToGrayscale(s_rgb_buf, s_gray_buf,
                           OV2640_WIDTH * OV2640_HEIGHT);

    static int fc = 0;
    if (fc++ % 10 == 0) {
        memcpy(s_working_buf, s_gray_buf, OV2640_WIDTH * OV2640_HEIGHT);
        Preprocess_Equalize(s_working_buf, OV2640_WIDTH * OV2640_HEIGHT);
    }

    TrackState_t state = Tracker_Process(&s_tracker, 0, s_gray_buf);

    float x_mm = 0, y_mm = 0;
    uint8_t lost = LOST_TARGET;
    Tracker_GetResult(&s_tracker, &x_mm, &y_mm, &lost);

    if (state != TRACK_STATE_IDLE) {
        Protocol_SendTrackResult(s_tracker.target_id, x_mm, y_mm, lost);
    }

    /* 广播/发送跟踪数据（统一频率：每3帧发送一次） */
    char json_buf[192];
    int n = snprintf(json_buf, sizeof(json_buf),
        "{\"s\":\"%s\",\"t\":%d,\"x\":%.0f,\"y\":%.0f,\"l\":%d,\"state\":%d}",
        s_is_server ? "A" : "B",
        s_tracker.target_id, (double)x_mm, (double)y_mm, lost, (int)state);

    if (n > 0 && n < (int)sizeof(json_buf)) {
        static int bc = 0;
        if (++bc % 3 == 0) {
            if (s_is_server && Network_ClientCount() > 0) {
                Network_BroadcastText(json_buf, n);
            } else if (!s_is_server) {
                Network_SendJSON(json_buf, n);
            }
        }
    }

    vTaskDelay(pdMS_TO_TICKS(33));
}
