/**
 * @file capture.h
 * @brief OV2640 图像采集封装
 *
 * 接口: Capture_Init, Capture_Frame
 */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 图像尺寸 ──── */
#define CAPTURE_WIDTH       160
#define CAPTURE_HEIGHT      120
#define CAPTURE_FRAME_SIZE  (CAPTURE_WIDTH * CAPTURE_HEIGHT)

/* ──── 公共接口 ──── */

/**
 * @brief 初始化图像采集
 * @return 0=成功, -1=失败
 */
int Capture_Init(void);

/**
 * @brief 采集一帧图像
 * @param buf   输出帧缓冲区
 * @param w     期望宽度 (应与 CAPTURE_WIDTH 一致)
 * @param h     期望高度 (应与 CAPTURE_HEIGHT 一致)
 * @return 0=成功, -1=失败
 */
int Capture_Frame(uint8_t *buf, int w, int h);

#ifdef __cplusplus
}
#endif

#endif /* CAPTURE_H */
