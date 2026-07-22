/**
 * @file relock.h
 * @brief 丢失复锁：扩大搜索窗口，重新定位
 *
 * 接口: Relock_Init, Relock_Execute
 */

#ifndef RELOCK_H
#define RELOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 公共接口 ──── */

/**
 * @brief 初始化复锁模块
 */
void Relock_Init(void);

/**
 * @brief 执行复锁搜索
 * @param frame  当前灰度图像
 * @param w      图像宽度
 * @param h      图像高度
 * @param out_x  输出目标 X (mm)
 * @param out_y  输出目标 Y (mm)
 * @return 0=复锁成功, -1=复锁失败
 */
int Relock_Execute(const uint8_t *frame, int w, int h, float *out_x, float *out_y);

/**
 * @brief 设置复锁区域 (扩大搜索范围)
 * @param expand_ratio 扩大比例 (2.0 = 2倍范围)
 */
void Relock_SetExpandRatio(float expand_ratio);

/**
 * @brief 在指定像素坐标附近执行复锁（优先缩小范围，避免全图粗扫）
 * @param frame   当前灰度图像
 * @param w       图像宽度
 * @param h       图像高度
 * @param cx      预测中心像素 X (从 H7 angle/dist 转换得来, -1=未知)
 * @param cy      预测中心像素 Y (-1=未知)
 * @param out_x   输出目标 X (mm)
 * @param out_y   输出目标 Y (mm)
 * @return 0=成功, -1=失败（降级到 Relock_Execute 全图扫）
 */
int Relock_ExecuteAt(const uint8_t *frame, int w, int h,
                     int cx, int cy,
                     float *out_x, float *out_y);

#ifdef __cplusplus
}
#endif

#endif /* RELOCK_H */
