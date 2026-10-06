/**
 * @file ov2640.h
 * @brief GOOUUU ESP32-S3-CAM N16R8板载OV2640驱动 (160x120, RGB565/GRAYSCALE)
 *
 * 接口: OV2640_Init, OV2640_Capture
 * 使用 esp32-camera 库或 GPIO 直接控制 DVP 并口。
 * 这里实现基于 espressif/esp32-camera 组件 (esp_camera.h)。
 */

#ifndef OV2640_H
#define OV2640_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 图像参数 ──── */
#define OV2640_WIDTH        160
#define OV2640_HEIGHT       120
#define OV2640_FRAME_SIZE   (OV2640_WIDTH * OV2640_HEIGHT)

/* 像素格式 */
#define OV2640_PIXFORMAT_RGB565     0   /* 2 bytes/pixel */
#define OV2640_PIXFORMAT_GRAYSCALE  1   /* 1 byte/pixel */

/* ──── 引脚配置 (需根据实际硬件修改) ──── */
typedef struct {
    int pin_pwdn;       /* Power Down */
    int pin_reset;      /* Reset */
    int pin_xclk;       /* XCLK */
    int pin_sscb_sda;   /* SCCB SDA */
    int pin_sscb_scl;   /* SCCB SCL */
    int pin_d7;         /* DVP D7 */
    int pin_d6;         /* DVP D6 */
    int pin_d5;         /* DVP D5 */
    int pin_d4;         /* DVP D4 */
    int pin_d3;         /* DVP D3 */
    int pin_d2;         /* DVP D2 */
    int pin_d1;         /* DVP D1 */
    int pin_d0;         /* DVP D0 */
    int pin_vsync;      /* VSYNC */
    int pin_href;       /* HREF */
    int pin_pclk;       /* PCLK */
} OV2640_PinConfig_t;

/* ──── 公共接口 ──── */

/**
 * @brief 初始化 OV2640
 * @param pixformat 像素格式 (OV2640_PIXFORMAT_RGB565 或 OV2640_PIXFORMAT_GRAYSCALE)
 * @param pins      引脚配置指针
 * @return 0=成功, -1=失败
 */
int OV2640_Init(int pixformat, const OV2640_PinConfig_t *pins);

/**
 * @brief 采集一帧图像
 * @param buf  输出缓冲区
 * @param len  缓冲区字节数
 * @return 0=成功, -1=失败
 */
int OV2640_Capture(uint8_t *buf, size_t len);

/**
 * @brief 设置输出像素格式 (运行时切换)
 * @param pixformat OV2640_PIXFORMAT_RGB565 或 OV2640_PIXFORMAT_GRAYSCALE
 */
void OV2640_SetPixFormat(int pixformat);

#ifdef __cplusplus
}
#endif

#endif /* OV2640_H */
