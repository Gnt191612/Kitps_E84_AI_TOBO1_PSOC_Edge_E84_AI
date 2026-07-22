/**
 * @file roi_select.h
 * @brief 视野内均匀选取 3 个高特征区域 (ROI)
 *
 * 将图像均匀分区块，计算每个区块梯度特征值，取 top-3。
 * 接口: ROI_Select
 */

#ifndef ROI_SELECT_H
#define ROI_SELECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── ROI 结构 ──── */
typedef struct {
    int x;      /* 左上角 X */
    int y;      /* 左上角 Y */
    int w;      /* 宽度 */
    int h;      /* 高度 */
    float score;/* 特征评分 */
} ROI_t;

/* ──── 常量 ──── */
#define ROI_MAX_COUNT   3
#define ROI_DEFAULT_W   16    /* 默认 ROI 宽度 */
#define ROI_DEFAULT_H   16    /* 默认 ROI 高度 */

/* ──── 公共接口 ──── */

/**
 * @brief 选取高特征 ROI
 * @param img      输入灰度图像 (8-bit)
 * @param w        图像宽度
 * @param h        图像高度
 * @param rois     输出 ROI 数组 (至少 max_rois 个)
 * @param max_rois 最大候选 ROI 数量
 * @return 实际选取的 ROI 数量
 */
int ROI_Select(const uint8_t *img, int w, int h, ROI_t *rois, int max_rois);

/**
 * @brief 快速选取 ROI (使用梯度幅值图, 更高效)
 * @param grad_mag 梯度幅值图
 * @param w        图像宽度
 * @param h        图像高度
 * @param rois     输出 ROI 数组
 * @param max_rois 最大数量
 * @return 实际选取的 ROI 数量
 */
int ROI_SelectFast(const uint8_t *grad_mag, int w, int h, ROI_t *rois, int max_rois);

#ifdef __cplusplus
}
#endif

#endif /* ROI_SELECT_H */
