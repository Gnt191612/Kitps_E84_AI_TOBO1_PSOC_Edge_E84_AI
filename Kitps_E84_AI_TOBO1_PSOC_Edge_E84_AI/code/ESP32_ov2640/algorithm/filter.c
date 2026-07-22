/**
 * @file filter.c
 * @brief 均值/卡尔曼滤波实现
 */

#include "filter.h"
#include <string.h>
#include <math.h>

/* ──── 均值滤波 ──── */
void MeanFilter_Init(MeanFilter_t *filter)
{
    if (!filter) return;
    memset(filter, 0, sizeof(MeanFilter_t));
    filter->head = 0;
    filter->count = 0;
}

void MeanFilter_Update(MeanFilter_t *filter, float meas_x, float meas_y,
                       float *out_x, float *out_y)
{
    if (!filter) { if (out_x) *out_x = meas_x; if (out_y) *out_y = meas_y; return; }

    filter->buf_x[filter->head] = meas_x;
    filter->buf_y[filter->head] = meas_y;
    filter->head = (filter->head + 1) % FILTER_WINDOW_SIZE;
    if (filter->count < FILTER_WINDOW_SIZE) filter->count++;

    float sum_x = 0, sum_y = 0;
    for (int i = 0; i < filter->count; i++) {
        sum_x += filter->buf_x[i];
        sum_y += filter->buf_y[i];
    }
    if (out_x) *out_x = sum_x / filter->count;
    if (out_y) *out_y = sum_y / filter->count;
}

/* ──── 卡尔曼滤波 (简化 2D 位置+速度) ──── */

/* 4x4 矩阵乘法 C = A * B */
static void mat44_mul(const float A[4][4], const float B[4][4], float C[4][4])
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            C[i][j] = A[i][0]*B[0][j] + A[i][1]*B[1][j] + A[i][2]*B[2][j] + A[i][3]*B[3][j];
        }
    }
}

/* 4x4 矩阵加法 C = A + B */
static void mat44_add(const float A[4][4], const float B[4][4], float C[4][4])
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            C[i][j] = A[i][j] + B[i][j];
}

/* 4x4 标量乘法 */
static void mat44_scale(float A[4][4], float s)
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            A[i][j] *= s;
}

/* 4x4 转置 */
static void mat44_transpose(const float A[4][4], float AT[4][4])
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            AT[i][j] = A[j][i];
}

/* 4x1 矩阵乘法 C = A * B */
static void mat44_vec4_mul(const float A[4][4], const float B[4], float C[4])
{
    for (int i = 0; i < 4; i++) {
        C[i] = A[i][0]*B[0] + A[i][1]*B[1] + A[i][2]*B[2] + A[i][3]*B[3];
    }
}

/* 4x4 单位矩阵 */
static void mat44_identity(float A[4][4])
{
    memset(A, 0, 4 * 4 * sizeof(float));
    for (int i = 0; i < 4; i++) A[i][i] = 1.0f;
}

void KalmanFilter_Init(KalmanFilter_t *kf)
{
    if (!kf) return;
    memset(kf, 0, sizeof(KalmanFilter_t));
    kf->initialized = 0;
    kf->Q = 0.01f;   /* 过程噪声 */
    kf->R = 0.1f;    /* 测量噪声 */
    mat44_identity(kf->P);
    mat44_scale(kf->P, 100.0f);  /* 初始不确定度 */
}

void KalmanFilter_Update(KalmanFilter_t *kf, float meas_x, float meas_y,
                         float dt, float *out_x, float *out_y)
{
    if (!kf) { if (out_x) *out_x = meas_x; if (out_y) *out_y = meas_y; return; }

    if (!kf->initialized) {
        kf->x = meas_x;
        kf->y = meas_y;
        kf->vx = 0;
        kf->vy = 0;
        kf->initialized = 1;
        if (out_x) *out_x = meas_x;
        if (out_y) *out_y = meas_y;
        return;
    }

    /* 状态转移矩阵 F = [1 0 dt 0; 0 1 0 dt; 0 0 1 0; 0 0 0 1] */
    float F[4][4];
    mat44_identity(F);
    F[0][2] = dt;
    F[1][3] = dt;

    /* 预测: x = F * x */
    float x_pred[4] = { kf->x, kf->y, kf->vx, kf->vy };
    float x_new[4];
    mat44_vec4_mul(F, x_pred, x_new);

    /* 预测: P = F * P * F^T + Q */
    float FT[4][4];
    mat44_transpose(F, FT);

    float FP[4][4], FPF[4][4];
    mat44_mul(F, kf->P, FP);
    mat44_mul(FP, FT, FPF);

    float Q_mat[4][4];
    memset(Q_mat, 0, sizeof(Q_mat));
    Q_mat[0][0] = kf->Q;
    Q_mat[1][1] = kf->Q;
    Q_mat[2][2] = kf->Q * 10;
    Q_mat[3][3] = kf->Q * 10;

    float P_pred[4][4];
    mat44_add(FPF, Q_mat, P_pred);

    /* 测量矩阵 H = [1 0 0 0; 0 1 0 0] */
    /* 测量噪声 R */
    float R_mat[2][2] = { {kf->R, 0}, {0, kf->R} };

    /* 卡尔曼增益 K = P * H^T * (H * P * H^T + R)^{-1} */

    /* S = H * P * H^T + R (2x2) */
    float S[2][2];
    S[0][0] = P_pred[0][0] + kf->R;
    S[0][1] = P_pred[0][1];
    S[1][0] = P_pred[1][0];
    S[1][1] = P_pred[1][1] + kf->R;

    /* S 求逆 (2x2) */
    float det = S[0][0]*S[1][1] - S[0][1]*S[1][0];
    if (fabs(det) < 1e-10f) det = 1e-10f;
    float Sinv[2][2];
    Sinv[0][0] = S[1][1] / det;
    Sinv[0][1] = -S[0][1] / det;
    Sinv[1][0] = -S[1][0] / det;
    Sinv[1][1] = S[0][0] / det;

    /* K = P * H^T * Sinv  (4x2) */
    float K[4][2];
    K[0][0] = (P_pred[0][0] * Sinv[0][0] + P_pred[0][1] * Sinv[1][0]);
    K[0][1] = (P_pred[0][0] * Sinv[0][1] + P_pred[0][1] * Sinv[1][1]);
    K[1][0] = (P_pred[1][0] * Sinv[0][0] + P_pred[1][1] * Sinv[1][0]);
    K[1][1] = (P_pred[1][0] * Sinv[0][1] + P_pred[1][1] * Sinv[1][1]);
    K[2][0] = (P_pred[2][0] * Sinv[0][0] + P_pred[2][1] * Sinv[1][0]);
    K[2][1] = (P_pred[2][0] * Sinv[0][1] + P_pred[2][1] * Sinv[1][1]);
    K[3][0] = (P_pred[3][0] * Sinv[0][0] + P_pred[3][1] * Sinv[1][0]);
    K[3][1] = (P_pred[3][0] * Sinv[0][1] + P_pred[3][1] * Sinv[1][1]);

    /* 更新: x = x_pred + K * (z - H * x_pred) */
    float innov[2];
    innov[0] = meas_x - x_new[0];
    innov[1] = meas_y - x_new[1];

    kf->x = x_new[0] + K[0][0]*innov[0] + K[0][1]*innov[1];
    kf->y = x_new[1] + K[1][0]*innov[0] + K[1][1]*innov[1];
    kf->vx = x_new[2] + K[2][0]*innov[0] + K[2][1]*innov[1];
    kf->vy = x_new[3] + K[3][0]*innov[0] + K[3][1]*innov[1];

    /* 更新: P = (I - K*H) * P_pred */
    float IKH[4][4];
    mat44_identity(IKH);
    IKH[0][0] -= K[0][0];
    IKH[0][1] -= K[0][1];
    IKH[1][0] -= K[1][0];
    IKH[1][1] -= K[1][1];
    IKH[2][0] -= K[2][0];
    IKH[2][1] -= K[2][1];
    IKH[3][0] -= K[3][0];
    IKH[3][1] -= K[3][1];

    mat44_mul(IKH, P_pred, kf->P);

    if (out_x) *out_x = kf->x;
    if (out_y) *out_y = kf->y;
}
