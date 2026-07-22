/**
 * @file roi_select.c
 * @brief ROI 选取实现：将图像均匀分区块，计算梯度特征，取 top-3
 */

#include "roi_select.h"
#include "Vision/feature.h"
#include <string.h>
#include <stdlib.h>

/* ──── 将图像均匀划分为六块 (3列 x 2行), 选每块内最佳子区域 ──── */
#define GRID_COLS   3
#define GRID_ROWS   2

/* ──── 简单插入排序, 取 top N ──── */
static void insert_sort_top(ROI_t *arr, int *count, int max_count, const ROI_t *candidate)
{
    if (*count < max_count) {
        arr[*count] = *candidate;
        (*count)++;
        /* 冒泡排序 */
        for (int i = *count - 1; i > 0; i--) {
            if (arr[i].score > arr[i-1].score) {
                ROI_t tmp = arr[i];
                arr[i] = arr[i-1];
                arr[i-1] = tmp;
            } else {
                break;
            }
        }
    } else if (candidate->score > arr[max_count - 1].score) {
        arr[max_count - 1] = *candidate;
        /* 重新排序 */
        for (int i = max_count - 1; i > 0; i--) {
            if (arr[i].score > arr[i-1].score) {
                ROI_t tmp = arr[i];
                arr[i] = arr[i-1];
                arr[i-1] = tmp;
            } else {
                break;
            }
        }
    }
}

/* ──── 使用原始图像计算出特征值 ──── */
int ROI_Select(const uint8_t *img, int w, int h, ROI_t *rois, int max_rois)
{
    if (!img || !rois || max_rois <= 0) return 0;

    /* 第一步: 计算梯度幅值图 */
    uint8_t *mag = (uint8_t *)malloc(w * h);
    if (!mag) return 0;

    Feature_CalcGradientMag(img, w, h, mag);

    int ret = ROI_SelectFast(mag, w, h, rois, max_rois);

    free(mag);
    return ret;
}

/* ──── 使用梯度幅值图快速选取 ──── */
int ROI_SelectFast(const uint8_t *grad_mag, int w, int h, ROI_t *rois, int max_rois)
{
    if (!grad_mag || !rois || max_rois <= 0) return 0;

    if (max_rois > ROI_MAX_COUNT) max_rois = ROI_MAX_COUNT;

    int roi_w = ROI_DEFAULT_W;
    int roi_h = ROI_DEFAULT_H;
    int count = 0;

    /* 将图像分成 GRID_COLS x GRID_ROWS 的网格 */
    int cell_w = w / GRID_COLS;
    int cell_h = h / GRID_ROWS;

    for (int gy = 0; gy < GRID_ROWS; gy++) {
        for (int gx = 0; gx < GRID_COLS; gx++) {
            int cell_x = gx * cell_w;
            int cell_y = gy * cell_h;

            /* 在该网格内以 stride 滑动窗口寻找最优子区域 */
            float best_score = 0;
            int best_x = cell_x;
            int best_y = cell_y;

            int step = 4;  /* 搜索步长 */
            for (int y = cell_y; y < cell_y + cell_h - roi_h; y += step) {
                for (int x = cell_x; x < cell_x + cell_w - roi_w; x += step) {
                    /* 计算窗口内平均梯度 */
                    float sum = 0;
                    for (int j = y; j < y + roi_h; j++) {
                        for (int i = x; i < x + roi_w; i++) {
                            sum += grad_mag[j * w + i];
                        }
                    }
                    float score = sum / (roi_w * roi_h);

                    if (score > best_score) {
                        best_score = score;
                        best_x = x;
                        best_y = y;
                    }
                }
            }

            /* 避免 ROI 重叠过近: 检查现有 ROI */
            int too_close = 0;
            for (int k = 0; k < count; k++) {
                int dx = abs(rois[k].x - best_x);
                int dy = abs(rois[k].y - best_y);
                if (dx < roi_w && dy < roi_h) {
                    too_close = 1;
                    break;
                }
            }

            if (!too_close && best_score > 10.0f) {
                ROI_t roi;
                roi.x = best_x;
                roi.y = best_y;
                roi.w = roi_w;
                roi.h = roi_h;
                roi.score = best_score;
                insert_sort_top(rois, &count, max_rois, &roi);
            }
        }
    }

    return count;
}
