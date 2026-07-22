/**
 * @file track_algorithm.h
 * @brief 跟踪算法封装：梯度 + 预测 + 滤波组合
 *
 * 接口: TrackAlgo_Init, TrackAlgo_Process
 */

#ifndef TRACK_ALGORITHM_H
#define TRACK_ALGORITHM_H

#include <stdint.h>
#include "algorithm/filter.h"
#include "algorithm/predict.h"
#include "Vision/roi/roi_select.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 跟踪算法句柄 ──── */
typedef struct {
    MeanFilter_t mean_filter;
    KalmanFilter_t kalman_filter;
    Predictor_t predictor;
    ROI_t rois[ROI_MAX_COUNT];
    ROI_t prev_rois[ROI_MAX_COUNT];
    int roi_count;
    int frame_count;
    int use_kalman;     /* 0=均值滤波, 1=卡尔曼滤波 */
} TrackAlgo_t;

/* ──── 公共接口 ──── */

/**
 * @brief 初始化跟踪算法
 * @param algo 算法句柄
 */
void TrackAlgo_Init(TrackAlgo_t *algo);

/**
 * @brief 处理一帧图像
 * @param algo      算法句柄
 * @param frame     输入灰度图像帧
 * @param w         图像宽度
 * @param h         图像高度
 * @param result_x  输出目标 X 坐标 (mm)
 * @param result_y  输出目标 Y 坐标 (mm)
 * @return 0=成功, -1=跟踪失败 (目标丢失)
 */
int TrackAlgo_Process(TrackAlgo_t *algo, const uint8_t *frame, int w, int h,
                      float *result_x, float *result_y);

/**
 * @brief 重置跟踪算法 (释放进程时调用)
 * @param algo 算法句柄
 */
void TrackAlgo_Reset(TrackAlgo_t *algo);

#ifdef __cplusplus
}
#endif

#endif /* TRACK_ALGORITHM_H */
