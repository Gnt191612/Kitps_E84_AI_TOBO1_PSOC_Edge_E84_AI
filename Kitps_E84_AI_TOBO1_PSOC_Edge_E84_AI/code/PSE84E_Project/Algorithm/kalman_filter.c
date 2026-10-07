/**
 * @file    kalman_filter.c
 * @brief   卡尔曼滤波器实现（一维/二维）
 * 
 * 内部细节：
 *   - 一维：状态 = [位置, 速度]，测量 = 位置
 *   - 二维：状态 = [距离, 角度, 速度, 角速度]，测量 = [距离, 角度]
 *   - 仅使用标准矩阵运算，无动态内存分配
 */

#include "kalman_filter.h"
#include <string.h>
#include <math.h>

/*----------------------------------------------------------------------------
 * 一维卡尔曼滤波器
 *----------------------------------------------------------------------------*/
void Kalman1D_Init(Kalman1D_t *kf, float dt, float process_noise, float measure_noise)
{
    memset(kf, 0, sizeof(Kalman1D_t));
    kf->dt = dt;
    /* 初始状态 */
    kf->x = 0.0f;
    kf->v = 0.0f;

    /* 协方差矩阵初值 */
    kf->P[0][0] = 1.0f;
    kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f;
    kf->P[1][1] = 1.0f;

    /* 过程噪声 */
    float dt2 = dt * dt;
    float dt3 = dt2 * dt / 2.0f;
    float dt4 = dt2 * dt2 / 4.0f;
    kf->Q[0][0] = dt4 * process_noise;
    kf->Q[0][1] = dt3 * process_noise;
    kf->Q[1][0] = dt3 * process_noise;
    kf->Q[1][1] = dt2 * process_noise;

    /* 测量噪声 */
    kf->R = measure_noise;
}

float Kalman1D_Update(Kalman1D_t *kf, float measurement)
{
    /* 预测步骤 */
    float x_pred = kf->x + kf->v * kf->dt;
    float v_pred = kf->v;

    float P00 = kf->P[0][0] + kf->dt * (kf->P[1][0] + kf->P[0][1]) + kf->dt * kf->dt * kf->P[1][1] + kf->Q[0][0];
    float P01 = kf->P[0][1] + kf->dt * kf->P[1][1] + kf->Q[0][1];
    float P10 = kf->P[1][0] + kf->dt * kf->P[1][1] + kf->Q[1][0];
    float P11 = kf->P[1][1] + kf->Q[1][1];

    /* 更新步骤 */
    float S = P00 + kf->R;   /* 测量残差协方差 */
    float K0 = P00 / S;
    float K1 = P10 / S;

    float y = measurement - x_pred;   /* 新息 */

    kf->x = x_pred + K0 * y;
    kf->v = v_pred + K1 * y;

    kf->P[0][0] = (1 - K0) * P00;
    kf->P[0][1] = (1 - K0) * P01;
    kf->P[1][0] = P10 - K1 * P00;
    kf->P[1][1] = P11 - K1 * P01;

    return kf->x;   /* 返回滤波后的位置 */
}

void Kalman1D_Predict(Kalman1D_t *kf, float dt, float *pred_val, float *pred_vel)
{
    *pred_val = kf->x + kf->v * dt;
    *pred_vel = kf->v;
}

/*----------------------------------------------------------------------------
 * 二维卡尔曼滤波器
 *----------------------------------------------------------------------------*/
void Kalman2D_Init(Kalman2D_t *kf, float dt,
                   float process_noise_pos, float process_noise_vel,
                   float measure_noise_dist, float measure_noise_angle)
{
    memset(kf, 0, sizeof(Kalman2D_t));
    kf->dt = dt;

    /* 初始状态全零 */
    /* 协方差矩阵初始化为单位矩阵（简化版） */
    for (int i = 0; i < 4; i++) kf->P[i][i] = 1.0f;

    /* 过程噪声（假设位置和速度独立） */
    float q_pos = process_noise_pos;
    float q_vel = process_noise_vel;
    float dt2 = dt * dt;
    float dt3 = dt2 * dt / 2.0f;
    float dt4 = dt2 * dt2 / 4.0f;

    kf->Q[0][0] = dt4 * q_pos;   /* dist 过程噪声 */
    kf->Q[0][2] = dt3 * q_pos;
    kf->Q[1][1] = dt4 * q_pos;   /* angle 过程噪声 */
    kf->Q[1][3] = dt3 * q_pos;
    kf->Q[2][0] = dt3 * q_pos;
    kf->Q[2][2] = dt2 * q_vel;
    kf->Q[3][1] = dt3 * q_pos;
    kf->Q[3][3] = dt2 * q_vel;

    /* 测量噪声矩阵 */
    kf->R[0][0] = measure_noise_dist;
    kf->R[0][1] = 0.0f;
    kf->R[1][0] = 0.0f;
    kf->R[1][1] = measure_noise_angle;
}

void Kalman2D_Update(Kalman2D_t *kf, float dist_meas, float angle_meas,
                     float *out_dist, float *out_angle,
                     float *out_vel, float *out_ang_vel)
{
    /*
     * 解耦二维卡尔曼：距离和角度独立滤波。
     *
     * 注意：完整二维卡尔曼需要4x4矩阵求逆和乘法，
     * 本代码采用解耦方式处理，实际效果通常可接受。
     *
     * [BUG FIX 2026-10-06] 原版使用 static 局部 Kalman1D_t 变量，
     * 导致所有 Kalman2D_t 实例共用同一对内部滤波器，多目标跟踪时
     * 产生数据交叉污染。现改为在 Kalman2D_t 中嵌入独立的一维滤波器。
     */

    /* 首次调用时初始化内部一维滤波器 */
    if (!kf->inner_init) {
        Kalman1D_Init(&kf->kf_dist,  kf->dt, 0.1f, kf->R[0][0]);
        Kalman1D_Init(&kf->kf_angle, kf->dt, 0.1f, kf->R[1][1]);
        kf->inner_init = 1;
    } else {
        kf->kf_dist.dt  = kf->dt;
        kf->kf_angle.dt = kf->dt;
    }

    *out_dist  = Kalman1D_Update(&kf->kf_dist, dist_meas);
    *out_angle = Kalman1D_Update(&kf->kf_angle, angle_meas);

    /* 速度/角速度从内部状态获取 */
    *out_vel     = kf->kf_dist.v;
    *out_ang_vel = kf->kf_angle.v;

    /* 将结果回写到 kf 结构体以保持一致性 */
    kf->x[0] = *out_dist;
    kf->x[1] = *out_angle;
    kf->x[2] = *out_vel;
    kf->x[3] = *out_ang_vel;
}

void Kalman2D_Predict(Kalman2D_t *kf, float future_dt,
                      float *pred_dist, float *pred_angle)
{
    /* 基于当前状态和速度/角速度线性外推 */
    *pred_dist  = kf->x[0] + kf->x[2] * future_dt;
    *pred_angle = kf->x[1] + kf->x[3] * future_dt;
}