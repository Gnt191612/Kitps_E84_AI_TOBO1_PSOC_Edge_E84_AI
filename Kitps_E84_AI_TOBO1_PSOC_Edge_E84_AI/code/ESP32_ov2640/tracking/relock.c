/**
 * @file relock.c
 * @brief 丢失复锁实现
 */

#include "relock.h"
#include "Vision/feature.h"
#include "Vision/roi/roi_select.h"
#include "data_logger/logger.h"
#include <string.h>
#include <stdlib.h>

static float s_expand_ratio = 1.5f;  /* 默认扩大 1.5 倍 */

void Relock_Init(void)
{
    s_expand_ratio = 1.5f;
    LOG_INFO("Relock init, expand=%.1f", s_expand_ratio);
}

void Relock_SetExpandRatio(float expand_ratio)
{
    if (expand_ratio >= 1.0f && expand_ratio <= 4.0f) {
        s_expand_ratio = expand_ratio;
    }
}

/* ──── 计算梯度评分 (辅助函数) ──── */
static float calc_grad_score(const uint8_t *frame, int w, int h,
                              int rx, int ry, int rw, int rh)
{
    int x1 = (rx < 0) ? 0 : rx, y1 = (ry < 0) ? 0 : ry;
    int x2 = (rx + rw > w) ? w : rx + rw;
    int y2 = (ry + rh > h) ? h : ry + rh;
    if (x2 - x1 < 4 || y2 - y1 < 4) return 0.0f;
    float sum = 0; int count = 0;
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            int gx_v = (x > 0 && x < w-1) ?
                       (int)frame[y*w + x+1] - (int)frame[y*w + x-1] : 0;
            int gy_v = (y > 0 && y < h-1) ?
                       (int)frame[(y+1)*w + x] - (int)frame[(y-1)*w + x] : 0;
            sum += (float)(abs(gx_v) + abs(gy_v));
            count++;
        }
    }
    return (count > 0) ? sum / count : 0.0f;
}

/* ──── 在预测位置附近逐级扩大搜索 ──── */
int Relock_ExecuteAt(const uint8_t *frame, int w, int h,
                     int cx, int cy,
                     float *out_x, float *out_y)
{
    if (!frame || !out_x || !out_y) return -1;

    /* 没有预测位置 → 直接降级到全图扫 */
    if (cx < 0 || cy < 0 || cx >= w || cy >= h)
        return Relock_Execute(frame, w, h, out_x, out_y);

    /* 逐级扩大搜索区域: 中心 → 逐步扩大 */
    const int expand_levels[][2] = {
        {20, 20},    /* 第1级: 20x20 * expand_ratio */
        {40, 40},    /* 第2级 */
        {60, 50},    /* 第3级 */
        {80, 60},    /* 第4级 */
        {120, 80},   /* 第5级 */
    };
    int levels = sizeof(expand_levels) / sizeof(expand_levels[0]);

    float best_score = 0;
    float best_cx = 0, best_cy = 0;

    for (int L = 0; L < levels; L++) {
        int box_w = (int)(expand_levels[L][0] * s_expand_ratio);
        int box_h = (int)(expand_levels[L][1] * s_expand_ratio);

        /* 在 [cx-box_w/2 .. cx+box_w/2] 范围内用粗网格搜索 */
        int step = (box_w > 40) ? box_w / 4 : 8;
        int x0 = cx - box_w / 2, y0 = cy - box_h / 2;

        for (int gy = y0; gy < y0 + box_h; gy += step) {
            for (int gx = x0; gx < x0 + box_w; gx += step) {
                float s = calc_grad_score(frame, w, h, gx, gy, 12, 12);
                if (s > best_score) {
                    best_score = s;
                    best_cx = (float)(gx + 6);
                    best_cy = (float)(gy + 6);
                }
            }
        }

        /* 如果当前级找到足够好的候选，提前退出 */
        if (best_score >= 15.0f) {
            break;
        }
    }

    if (best_score < 10.0f) {
        /* 预测位置附近搜不到，降级全图扫 */
        LOG_WARN("RelockAt failed (score=%.1f), fallback to full scan", best_score);
        return Relock_Execute(frame, w, h, out_x, out_y);
    }

    const float PIXEL_TO_MM_X = 1.875f;
    const float PIXEL_TO_MM_Y = 1.875f;
    *out_x = best_cx * PIXEL_TO_MM_X;
    *out_y = best_cy * PIXEL_TO_MM_Y;

    LOG_INFO("RelockAt success: px=(%.0f,%.0f) mm=(%.1f,%.1f) score=%.1f",
             best_cx, best_cy, *out_x, *out_y, best_score);
    return 0;
}

int Relock_Execute(const uint8_t *frame, int w, int h, float *out_x, float *out_y)
{
    if (!frame || !out_x || !out_y) return -1;

    /* 全图网格扫描 (原有逻辑) */
    int roi_w = (int)(ROI_DEFAULT_W * s_expand_ratio);
    int roi_h = (int)(ROI_DEFAULT_H * s_expand_ratio);
    if (roi_w > w / 2) roi_w = w / 2;
    if (roi_h > h / 2) roi_h = h / 2;

    /* 扩大网格搜索范围: 使用更粗的步长覆盖全图 */
    ROI_t candidates[ROI_MAX_COUNT * 2];
    int candidate_count = 0;

    int step_x = w / 4;
    int step_y = h / 3;

    /* 在全图均匀采样区域找高特征区域 */
    for (int gy = 0; gy < h - roi_h; gy += step_y) {
        for (int gx = 0; gx < w - roi_w; gx += step_x) {
            /* 快速计算该区域梯度幅值 */
            float grad_sum = 0;
            for (int y = gy; y < gy + roi_h && y < h; y++) {
                for (int x = gx; x < gx + roi_w && x < w; x++) {
                    int gx_v = (x > 0 && x < w-1) ?
                               (int)frame[y*w + x+1] - (int)frame[y*w + x-1] : 0;
                    int gy_v = (y > 0 && y < h-1) ?
                               (int)frame[(y+1)*w + x] - (int)frame[(y-1)*w + x] : 0;
                    grad_sum += (float)(abs(gx_v) + abs(gy_v));
                }
            }

            ROI_t roi;
            roi.x = gx;
            roi.y = gy;
            roi.w = roi_w;
            roi.h = roi_h;
            roi.score = grad_sum / (roi_w * roi_h);

            if (candidate_count < ROI_MAX_COUNT * 2) {
                candidates[candidate_count++] = roi;
            }
        }
    }

    /* 找最高评分的区域 */
    if (candidate_count == 0) {
        LOG_WARN("Relock: no candidates found");
        return -1;
    }

    int best_idx = 0;
    for (int i = 1; i < candidate_count; i++) {
        if (candidates[i].score > candidates[best_idx].score) {
            best_idx = i;
        }
    }

    /* 如果最高分仍然太低, 认为失败 */
    if (candidates[best_idx].score < 10.0f) {
        LOG_WARN("Relock: best score too low (%.1f)", candidates[best_idx].score);
        return -1;
    }

    /* 像素转 mm */
    float cx = (float)(candidates[best_idx].x + candidates[best_idx].w / 2);
    float cy = (float)(candidates[best_idx].y + candidates[best_idx].h / 2);

    /* 使用与 track_algorithm.c 相同的像素转换系数 */
    const float PIXEL_TO_MM_X = 1.875f;
    const float PIXEL_TO_MM_Y = 1.875f;

    *out_x = cx * PIXEL_TO_MM_X;
    *out_y = cy * PIXEL_TO_MM_Y;

    LOG_INFO("Relock success: mm=(%.1f, %.1f), score=%.1f", *out_x, *out_y, candidates[best_idx].score);
    return 0;
}
