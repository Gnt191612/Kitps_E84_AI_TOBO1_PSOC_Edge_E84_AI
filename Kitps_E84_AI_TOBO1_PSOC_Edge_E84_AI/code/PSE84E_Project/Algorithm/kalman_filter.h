/**
 * @file    kalman_filter.h
 * @brief   一维/二维卡尔曼滤波器（84E本地轨迹平滑预判）
 * 
 * 公开接口：
 *   Kalman1D_Init()          - 初始化一维滤波器
 *   Kalman1D_Update()        - 输入测量值，输出滤波值和预测值
 *   Kalman2D_Init()          - 初始化二维滤波器（距离+角度）
 *   Kalman2D_Update()        - 输入测量值，输出滤波状态和预测状态
 */

#ifndef __KALMAN_FILTER_H
#define __KALMAN_FILTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 一维卡尔曼（用于距离或角度独立滤波） ---------- */
typedef struct {
    float x;          /* 状态值 */
    float v;          /* 速度（变化率） */
    float P[2][2];    /* 协方差矩阵 */
    float Q[2][2];    /* 过程噪声协方差 */
    float R;          /* 测量噪声协方差 */
    float dt;         /* 时间间隔（秒） */
} Kalman1D_t;

void  Kalman1D_Init(Kalman1D_t *kf, float dt, float process_noise, float measure_noise);
float Kalman1D_Update(Kalman1D_t *kf, float measurement);
void  Kalman1D_Predict(Kalman1D_t *kf, float dt, float *pred_val, float *pred_vel);

/* ---------- 二维卡尔曼（距离+角度联合滤波） ---------- */
typedef struct {
    float x[4];       /* 状态：[距离, 角度, 速度, 角速度] */
    float P[4][4];
    float Q[4][4];
    float R[2][2];    /* 测量噪声（距离测量噪声 + 角度测量噪声） */
    float dt;
    /* 内部一维滤波器实例（解耦实现） */
    Kalman1D_t kf_dist;
    Kalman1D_t kf_angle;
    uint8_t     inner_init;  /* 内部滤波器是否已初始化 */
} Kalman2D_t;

void Kalman2D_Init(Kalman2D_t *kf, float dt,
                   float process_noise_pos, float process_noise_vel,
                   float measure_noise_dist, float measure_noise_angle);
void Kalman2D_Update(Kalman2D_t *kf, float dist_meas, float angle_meas,
                     float *out_dist, float *out_angle,
                     float *out_vel, float *out_ang_vel);
void Kalman2D_Predict(Kalman2D_t *kf, float future_dt,
                      float *pred_dist, float *pred_angle);

#ifdef __cplusplus
}
#endif

#endif /* __KALMAN_FILTER_H */