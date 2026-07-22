/**
 * @file track_algorithm.c
 * @brief 跟踪算法封装实现
 */

#include "track_algorithm.h"
/* 外部函数: gradient.c 中的梯度跟踪 */
int Gradient_Track(const ROI_t *prev_roi, const uint8_t *curr_img,
                   int w, int h, ROI_t *new_roi);
#include "sys/time.h"
#include <string.h>
#include <math.h>

/* 像素到毫米的转换系数 (需根据实际标定)
 * 假设 160x120 @ 20cm 视场对应 ~300mm x 225mm */
#define PIXEL_TO_MM_X   1.875f   /* 300mm / 160px */
#define PIXEL_TO_MM_Y   1.875f   /* 225mm / 120px */

/* ──── 初始化 ──── */
void TrackAlgo_Init(TrackAlgo_t *algo)
{
    if (!algo) return;
    memset(algo, 0, sizeof(TrackAlgo_t));

    MeanFilter_Init(&algo->mean_filter);
    KalmanFilter_Init(&algo->kalman_filter);
    Predict_Init(&algo->predictor);

    algo->roi_count = 0;
    algo->frame_count = 0;
    algo->use_kalman = 1;  /* 默认使用卡尔曼 */
}

/* ──── 像素坐标转毫米 ──── */
static inline void pixel_to_mm(int px, int py, float *mm_x, float *mm_y)
{
    if (mm_x) *mm_x = (float)px * PIXEL_TO_MM_X;
    if (mm_y) *mm_y = (float)py * PIXEL_TO_MM_Y;
}

/* ──── 处理一帧 ──── */
int TrackAlgo_Process(TrackAlgo_t *algo, const uint8_t *frame, int w, int h,
                      float *result_x, float *result_y)
{
    if (!algo || !frame) return -1;

    algo->frame_count++;

    /* 第一帧: 选取初始 ROI */
    if (algo->frame_count == 1) {
        algo->roi_count = ROI_Select(frame, w, h, algo->rois, ROI_MAX_COUNT);
        memcpy(algo->prev_rois, algo->rois, algo->roi_count * sizeof(ROI_t));

        if (algo->roi_count == 0) return -1;

        /* 计算所有 ROI 的中心 */
        float cx_sum = 0, cy_sum = 0;
        for (int i = 0; i < algo->roi_count; i++) {
            cx_sum += algo->rois[i].x + algo->rois[i].w / 2.0f;
            cy_sum += algo->rois[i].y + algo->rois[i].h / 2.0f;
        }
        float cx = cx_sum / algo->roi_count;
        float cy = cy_sum / algo->roi_count;

        pixel_to_mm((int)cx, (int)cy, result_x, result_y);
        return 0;
    }

    /* 后续帧: 梯度跟踪每个 ROI */
    int tracked_count = 0;
    float cx_sum = 0, cy_sum = 0;

    for (int i = 0; i < algo->roi_count; i++) {
        ROI_t new_roi;
        int ret = Gradient_Track(&algo->prev_rois[i], frame, w, h, &new_roi);

        if (ret == 0) {
            algo->rois[tracked_count] = new_roi;
            cx_sum += new_roi.x + new_roi.w / 2.0f;
            cy_sum += new_roi.y + new_roi.h / 2.0f;
            tracked_count++;
        }
    }

    algo->roi_count = tracked_count;

    if (tracked_count == 0) return -1;   /* 完全丢失 */

    float cx = cx_sum / tracked_count;
    float cy = cy_sum / tracked_count;

    /* 保存当前 ROI 为下一帧的 prev */
    memcpy(algo->prev_rois, algo->rois, tracked_count * sizeof(ROI_t));

    /* 像素 → mm */
    float mm_x, mm_y;
    pixel_to_mm((int)cx, (int)cy, &mm_x, &mm_y);

    /* 应用预测和滤波 */
    if (algo->use_kalman) {
        static uint32_t prev_time = 0;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint32_t now_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

        float dt = (prev_time > 0) ? (now_ms - prev_time) / 1000.0f : 0.033f;
        if (dt > 0.5f) dt = 0.033f;  /* 限制最大 dt */
        prev_time = now_ms;

        /* 卡尔曼滤波 */
        KalmanFilter_Update(&algo->kalman_filter, mm_x, mm_y, dt, result_x, result_y);

        /* 预测下一帧 */
        float pred_x, pred_y;
        Predict_Update(&algo->predictor, *result_x, *result_y, dt, &pred_x, &pred_y);
        /* 这里保留滤波结果, 预测结果用于闭环控制 */
    } else {
        /* 均值滤波 */
        MeanFilter_Update(&algo->mean_filter, mm_x, mm_y, result_x, result_y);
    }

    return 0;
}

/* ──── 重置 ──── */
void TrackAlgo_Reset(TrackAlgo_t *algo)
{
    TrackAlgo_Init(algo);
}
