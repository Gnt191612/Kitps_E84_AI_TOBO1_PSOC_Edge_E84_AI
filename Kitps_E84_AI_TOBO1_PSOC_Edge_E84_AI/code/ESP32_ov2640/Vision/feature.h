/**
 * @file feature.h
 * @brief 特征提取：梯度计算
 *
 * 接口: Feature_CalcGradient
 */

#ifndef FEATURE_H
#define FEATURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 梯度结构 ──── */
typedef struct {
    float gx;   /* X 方向梯度 */
    float gy;   /* Y 方向梯度 */
    float mag;  /* 梯度幅值 */
    float ang;  /* 梯度方向 (弧度) */
} Gradient_t;

/* ──── 公共接口 ──── */

/**
 * @brief 计算图像梯度 (Sobel 3x3 近似)
 * @param img      输入灰度图像 (8-bit)
 * @param w        图像宽度
 * @param h        图像高度
 * @param grad_buf 输出梯度缓冲区 (Gradient_t 数组, w*h 个元素)
 */
void Feature_CalcGradient(const uint8_t *img, int w, int h, Gradient_t *grad_buf);

/**
 * @brief 计算图像梯度幅值 (快速版本, 使用 |gx|+|gy|)
 * @param img   输入灰度图像
 * @param w     宽度
 * @param h     高度
 * @param mag   输出幅值缓冲区 (uint8_t)
 */
void Feature_CalcGradientMag(const uint8_t *img, int w, int h, uint8_t *mag);

/**
 * @brief 计算指定区域内平均梯度幅值
 * @param grad_buf 梯度缓冲区
 * @param w        图像宽度
 * @param h        图像高度
 * @param x        区域左上 X
 * @param y        区域左上 Y
 * @param rw       区域宽度
 * @param rh       区域高度
 * @return 平均梯度幅值
 */
float Feature_RegionMeanMag(const Gradient_t *grad_buf, int w, int h,
                            int x, int y, int rw, int rh);

#ifdef __cplusplus
}
#endif

#endif /* FEATURE_H */
