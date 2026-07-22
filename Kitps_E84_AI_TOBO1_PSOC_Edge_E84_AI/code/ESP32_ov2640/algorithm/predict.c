/**
 * @file predict.c
 * @brief 目标运动预测实现
 */

#include "predict.h"
#include <string.h>

void Predict_Init(Predictor_t *pred)
{
    if (!pred) return;
    memset(pred, 0, sizeof(Predictor_t));
    pred->initialized = 0;
}

void Predict_Update(Predictor_t *pred, float pos_x, float pos_y, float dt,
                    float *pred_x, float *pred_y)
{
    if (!pred) {
        if (pred_x) *pred_x = pos_x;
        if (pred_y) *pred_y = pos_y;
        return;
    }

    if (!pred->initialized) {
        pred->pos_x = pos_x;
        pred->pos_y = pos_y;
        pred->prev_x = pos_x;
        pred->prev_y = pos_y;
        pred->vel_x = 0;
        pred->vel_y = 0;
        pred->initialized = 1;
        if (pred_x) *pred_x = pos_x;
        if (pred_y) *pred_y = pos_y;
        return;
    }

    /* 更新速度 (使用低通滤波) */
    const float alpha = 0.7f;  /* 平滑系数 */
    if (dt > 0.001f) {
        float inst_vel_x = (pos_x - pred->prev_x) / dt;
        float inst_vel_y = (pos_y - pred->prev_y) / dt;

        pred->vel_x = alpha * inst_vel_x + (1 - alpha) * pred->vel_x;
        pred->vel_y = alpha * inst_vel_y + (1 - alpha) * pred->vel_y;
    }

    /* 保存上一帧位置 */
    pred->prev_x = pred->pos_x;
    pred->prev_y = pred->pos_y;

    /* 更新当前位置 */
    pred->pos_x = pos_x;
    pred->pos_y = pos_y;

    /* 预测下一帧位置 (假设 dt 不变) */
    if (pred_x) *pred_x = pred->pos_x + pred->vel_x * dt;
    if (pred_y) *pred_y = pred->pos_y + pred->vel_y * dt;
}

void Predict_Reset(Predictor_t *pred)
{
    if (pred) {
        memset(pred, 0, sizeof(Predictor_t));
        pred->initialized = 0;
    }
}
