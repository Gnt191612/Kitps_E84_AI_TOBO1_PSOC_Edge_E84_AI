/**
 * @file preprocess.h
 * @brief 图像预处理: 灰度转换、直方图均衡、缩放
 *
 * 接口: Preprocess_ToGrayscale, Preprocess_Equalize, Preprocess_Scale
 */

#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 公共接口 ──── */

/**
 * @brief RGB565 转灰度 (8-bit)
 * @param src  输入 RGB565 缓冲区 (2 bytes/pixel)
 * @param dst  输出灰度缓冲区 (1 byte/pixel)
 * @param len  像素数量
 */
void Preprocess_ToGrayscale(const uint8_t *src, uint8_t *dst, int len);

/**
 * @brief 直方图均衡 (增强对比度)
 * @param img  输入/输出灰度图像 (8-bit)
 * @param len  像素数量
 */
void Preprocess_Equalize(uint8_t *img, int len);

/**
 * @brief 图像缩放 (最近邻插值)
 * @param src    输入图像
 * @param src_w  输入宽度
 * @param src_h  输入高度
 * @param dst    输出图像
 * @param dst_w  目标宽度
 * @param dst_h  目标高度
 */
void Preprocess_Scale(const uint8_t *src, int src_w, int src_h,
                      uint8_t *dst, int dst_w, int dst_h);

/**
 * @brief 简单二值化 (Otsu 法)
 * @param img   输入灰度图像
 * @param len   像素数量
 * @param out   输出二值图像 (0/255)
 */
void Preprocess_Binarize(const uint8_t *img, uint8_t *out, int len);

#ifdef __cplusplus
}
#endif

#endif /* PREPROCESS_H */
