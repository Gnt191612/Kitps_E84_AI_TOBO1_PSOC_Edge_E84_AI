/**
 * @file filter.h
 * @brief 简单卡尔曼/均值滤波
 *
 * 接口: Filter_Init, Filter_Update
 * 实现一个一阶低通/均值滤波器, 可选简单卡尔曼滤波。
 */

#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 均值滤波器 ──── */
#define FILTER_WINDOW_SIZE  5

typedef struct {
    float buf_x[FILTER_WINDOW_SIZE];
    float buf_y[FILTER_WINDOW_SIZE];
    int head;
    int count;
} MeanFilter_t;

/* ──── 简单卡尔曼滤波器 ──── */
typedef struct {
    float x;        /* 状态: x 位置 */
    float y;        /* 状态: y 位置 */
    float vx;       /* 状态: x 速度 */
    float vy;       /* 状态: y 速度 */
    float P[4][4];  /* 协方差矩阵 (4x4) */
    float Q;        /* 过程噪声 */
    float R;        /* 测量噪声 */
    int initialized;
} KalmanFilter_t;

/* ──── 公共接口 ──── */

/**
 * @brief 初始化均值滤波器
 * @param filter 滤波器指针
 */
void MeanFilter_Init(MeanFilter_t *filter);

/**
 * @brief 均值滤波更新
 * @param filter 滤波器指针
 * @param meas_x 测量 X
 * @param meas_y 测量 Y
 * @param out_x  输出滤波 X
 * @param out_y  输出滤波 Y
 */
void MeanFilter_Update(MeanFilter_t *filter, float meas_x, float meas_y,
                       float *out_x, float *out_y);

/**
 * @brief 初始化卡尔曼滤波器
 * @param kf 卡尔曼滤波器指针
 */
void KalmanFilter_Init(KalmanFilter_t *kf);

/**
 * @brief 卡尔曼滤波更新
 * @param kf     卡尔曼滤波器指针
 * @param meas_x 测量 X
 * @param meas_y 测量 Y
 * @param dt     时间步长 (秒)
 * @param out_x  输出 X
 * @param out_y  输出 Y
 */
void KalmanFilter_Update(KalmanFilter_t *kf, float meas_x, float meas_y,
                         float dt, float *out_x, float *out_y);

#ifdef __cplusplus
}
#endif

#endif /* FILTER_H */
