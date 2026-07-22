/**
 * @file    radar.c
 * @brief   雷达控制代理实现 — 通过 UART 与 STM32H7 通信
 *
 * 注意：HC-SR04 超声波雷达物理连接在 STM32H7 上，
 *       PSE84E 通过 UART 协议向 H7 发送雷达指令并接收回传数据。
 *
 * 通信流程：
 *   1. Radar_SetAngle(angle)  → 打包成命令帧通过 UART 发给 H7
 *   2. H7 执行舵机旋转 + HC-SR04 测距
 *   3. H7 回传结果帧（含距离/强度）
 *   4. Radar_GetEchoStrength() 返回上次缓存的结果
 *
 * 协议帧格式与现有 protocol.h / frame.h 一致：
 *   FRAME_TYPE_RADAR_CMD = 0x10
 *   FRAME_TYPE_RADAR_RES = 0x11
 */

#include "radar.h"
#include "communication.h"
#include "protocol.h"
#include "frame.h"
#include "logger.h"
#include "XMC8400E.h"

#include <string.h>
#include <stdbool.h>

/* ======================================================================== */
/* 雷达协议帧类型（与 H7 约定）                                                */
/* ======================================================================== */
#define FRAME_TYPE_RADAR_CMD  0x10   /* PSE84E → H7: 雷达指令 */
#define FRAME_TYPE_RADAR_RES  0x11   /* H7 → PSE84E: 雷达结果 */

/* 雷达指令类型 */
#define RADAR_CMD_SET_ANGLE    0x01   /* 设置舵机角度 */
#define RADAR_CMD_TRIGGER      0x02   /* 触发测距 */

/* ======================================================================== */
/* 雷达命令/结果负载结构体（与 H7 约定，packed）                                */
/* ======================================================================== */
#pragma pack(push, 1)
typedef struct {
    uint8_t  cmd;           /* RADAR_CMD_SET_ANGLE / RADAR_CMD_TRIGGER */
    float    angle_deg;     /* 目标角度（仅 SET_ANGLE 有效） */
} RadarCmdPayload_t;

typedef struct {
    float    distance_cm;   /* 测量距离 (cm) */
    float    strength;      /* 回波强度 0~1 */
    uint8_t  valid;         /* 0=无效, 1=有效 */
} RadarResPayload_t;
#pragma pack(pop)

/* ======================================================================== */
/* 内部状态                                                                  */
/* ======================================================================== */
static volatile float g_last_distance = 0.0f;     /* 上次测量结果缓存 */
static volatile float g_last_strength = 0.0f;
static volatile bool  g_result_ready  = false;
static volatile bool  g_angle_done    = false;     /* 舵机到位标记 */
static volatile uint32_t g_last_cmd_seq = 0;       /* 命令序列号 */
static uint32_t g_cmd_seq = 0;

/* ======================================================================== */
/* UART 接收回调 — H7 回传的数据帧由此解析                                    */
/* ======================================================================== */
static void radar_rx_frame(uint8_t type, uint8_t *payload, uint16_t len)
{
    if (type == FRAME_TYPE_RADAR_RES && payload && len >= sizeof(RadarResPayload_t)) {
        RadarResPayload_t *res = (RadarResPayload_t *)payload;
        if (res->valid) {
            g_last_distance = res->distance_cm;
            g_last_strength = res->strength;
        } else {
            g_last_distance = 0.0f;
            g_last_strength = 0.0f;
        }
        g_result_ready = true;
    }
    /* 可扩展其他回执帧类型 */
}

/**
 * @brief 注册到 protocol.c 的帧分发回调（由 Protocol_RegisterFrameCallback 调用）
 *        在 protocol.c 的帧解析完成后回调到此函数。
 */
void Radar_RegisterProtocol(void)
{
    /* 目前通过 Protocol_RegisterFrameCallback 注册，详见 protocol.c 的扩展 */
    /* 此处暂用直接注册方式，框架会在 main.h 中声明 extern */
}

/* ======================================================================== */
/* 发送命令到 H7，等待回执（阻塞，带超时）                                      */
/* ======================================================================== */
static bool send_cmd_and_wait(uint8_t cmd_type, float angle, uint32_t timeout_ms)
{
    uint8_t frame_buf[32];
    uint16_t frame_len;

    /* 构造负载 */
    RadarCmdPayload_t cmd_payload;
    cmd_payload.cmd      = cmd_type;
    cmd_payload.angle_deg = angle;

    /* 打包帧并发送 */
    if (Frame_Pack(frame_buf, &frame_len, FRAME_TYPE_RADAR_CMD,
                   (uint8_t *)&cmd_payload, sizeof(cmd_payload)) != 0) {
        return false;
    }

    g_result_ready = false;
    g_angle_done   = false;
    g_last_cmd_seq = ++g_cmd_seq;

    Comm_SendData(frame_buf, frame_len);

    /* 等待 H7 回执（简易忙等待，实际可用超时循环） */
    uint32_t start = *((volatile uint32_t *)0xE0001004); /* DWT CYCCNT */
    uint32_t timeout_ticks = timeout_ms * (160000000UL / 1000UL);

    while (1) {
        /* 如果有新结果帧到达 */
        if (g_result_ready) {
            return true;
        }
        uint32_t now = *((volatile uint32_t *)0xE0001004);
        if ((now - start) > timeout_ticks) {
            Logger_Print(LOG_WARN, "Radar cmd timeout (cmd=%d)", cmd_type);
            return false;
        }
    }
}

/* ======================================================================== */
/* 公开接口                                                                  */
/* ======================================================================== */

void Radar_Init(void)
{
    g_last_distance = 0.0f;
    g_last_strength = 0.0f;
    g_result_ready  = false;
    g_angle_done    = false;

    /* 注册帧回调，接收 H7 回传的雷达数据 */
    Protocol_RegisterFrameCallback(radar_rx_frame);

    Logger_Print(LOG_INFO, "Radar proxy (H7 UART) initiated");
}

/**
 * @brief 设置雷达扫描角度 — 通过 UART 通知 H7 控制舵机
 *        阻塞等待 H7 确认角度到位
 */
void Radar_SetAngle(float angle_deg)
{
    send_cmd_and_wait(RADAR_CMD_SET_ANGLE, angle_deg, 500);
}

/**
 * @brief 触发 HC-SR04 测距并获取回波强度
 *        通过 UART 请求 H7 执行测距，返回缓存强度值
 */
float Radar_GetEchoStrength(void)
{
    send_cmd_and_wait(RADAR_CMD_TRIGGER, 0.0f, 100);
    return g_last_strength;
}

/**
 * @brief 获取最近一次测量的距离（cm）
 */
float Radar_GetDistance(void)
{
    return g_last_distance;
}
