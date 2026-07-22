/**
 * @file ov2640.c
 * @brief OV2640 摄像头驱动实现 (基于 espressif/esp32-camera)
 */

#include "ov2640.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>
/* camera_io.h 放在 esp_camera 之后，避免 #define OV2640_SCCB_ADDR 与
 * esp_camera/sensor.h 枚举定义冲突 */
#include "camera_io.h"

static const char *TAG = "OV2640";

static int s_pixformat = OV2640_PIXFORMAT_RGB565;

/* ──── 默认引脚配置 (盈的连线方案) ──── */
/*
 * PCLK→GPIO22 避开 PWM Tilt(GPIO14) 冲突
 * HREF→GPIO35 (输入专用) 避开 GPIO13
 * PWDN/RESET 接可控 GPIO 避免浮动
 */
static const OV2640_PinConfig_t s_default_pins = {
    .pin_pwdn      = 32,   /* 低电平工作 */
    .pin_reset     = 33,   /* 外部拉低再释放复位 */
    .pin_xclk      = 4,
    .pin_sscb_sda  = 18,
    .pin_sscb_scl  = 23,
    .pin_d7        = 16,
    .pin_d6        = 5,
    .pin_d5        = 17,
    .pin_d4        = 21,
    .pin_d3        = 19,
    .pin_d2        = 26,
    .pin_d1        = 25,
    .pin_d0        = 34,   /* 输入专用 OK */
    .pin_vsync     = 27,
    .pin_href      = 35,   /* 输入专用 OK */
    .pin_pclk      = 22,   /* 避开 GPIO14(PWM Tilt) */
};

/* ──── 初始化 ──── */
int OV2640_Init(int pixformat, const OV2640_PinConfig_t *pins)
{
    if (!pins) pins = &s_default_pins;
    s_pixformat = pixformat;

    /* 配置 camera */
    camera_config_t config = {
        .pin_pwdn  = pins->pin_pwdn,
        .pin_reset = pins->pin_reset,
        .pin_xclk = pins->pin_xclk,
        .pin_sscb_sda = pins->pin_sscb_sda,
        .pin_sscb_scl = pins->pin_sscb_scl,

        .pin_d7 = pins->pin_d7,
        .pin_d6 = pins->pin_d6,
        .pin_d5 = pins->pin_d5,
        .pin_d4 = pins->pin_d4,
        .pin_d3 = pins->pin_d3,
        .pin_d2 = pins->pin_d2,
        .pin_d1 = pins->pin_d1,
        .pin_d0 = pins->pin_d0,
        .pin_vsync = pins->pin_vsync,
        .pin_href = pins->pin_href,
        .pin_pclk = pins->pin_pclk,

        /* XCLK 频率 */
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        /* 分辨率: 160x120 (QQVGA) */
        .pixel_format = (pixformat == OV2640_PIXFORMAT_GRAYSCALE) ?
                         PIXFORMAT_GRAYSCALE : PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_QQVGA,  /* 160x120 */

        /* 单帧缓存 */
        .jpeg_quality = 12,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    /* 如果无 PSRAM, fallback 到内部 DRAM */
#if !CONFIG_ESP32_SPIRAM_SUPPORT
    config.fb_location = CAMERA_FB_IN_DRAM;
#endif

    esp_err_t ret = esp_camera_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %d", ret);
        return -1;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        /* 设置 OV2640 输出格式 */
        s->set_pixformat(s, (pixformat == OV2640_PIXFORMAT_GRAYSCALE) ?
                            PIXFORMAT_GRAYSCALE : PIXFORMAT_RGB565);
        s->set_framesize(s, FRAMESIZE_QQVGA);
    }

    ESP_LOGI(TAG, "OV2640 init OK, format=%d, %dx%d", pixformat, OV2640_WIDTH, OV2640_HEIGHT);
    return 0;
}

/* ──── 采集一帧 ──── */
int OV2640_Capture(uint8_t *buf, size_t len)
{
    if (!buf) return -1;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return -1;
    }

    size_t copy_len = (fb->len < len) ? fb->len : len;
    memcpy(buf, fb->buf, copy_len);
    esp_camera_fb_return(fb);

    return (int)copy_len;
}

/* ──── 设置像素格式 ──── */
void OV2640_SetPixFormat(int pixformat)
{
    s_pixformat = pixformat;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_pixformat(s, (pixformat == OV2640_PIXFORMAT_GRAYSCALE) ?
                            PIXFORMAT_GRAYSCALE : PIXFORMAT_RGB565);
    }
}
