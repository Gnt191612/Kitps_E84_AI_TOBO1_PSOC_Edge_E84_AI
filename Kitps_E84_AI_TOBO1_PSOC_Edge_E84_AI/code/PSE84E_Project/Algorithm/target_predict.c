/**
 * @file    target_predict.c
 * @brief   目标预判算法实现
 * 
 * 算法要点：
 *   - 采用平滑窗口分析距离和角度的变化率、方差
 *   - 搜索窗宽度 = 基础宽度 + 预测不确定性（基于速度波动）
 *   - 预判中心角 = 当前角度 + 角速度 * 预测时间
 */

#include "target_predict.h"
#include <string.h>
#include <math.h>

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void TargetPredict_Init(TargetPredict_t *pred)
{
    memset(pred, 0, sizeof(TargetPredict_t));
}

/*----------------------------------------------------------------------------
 * 更新历史记录并计算统计量
 *----------------------------------------------------------------------------*/
void TargetPredict_UpdateHistory(TargetPredict_t *pred, float dist, float angle, uint32_t time_ms)
{
    uint8_t idx = pred->index;
    pred->dist_history[idx]  = dist;
    pred->angle_history[idx] = angle;
    pred->time_ms[idx]       = time_ms;

    if (!pred->full) {
        if (idx == PREDICT_HISTORY_LEN - 1) {
            pred->full = 1;
        }
    }
    pred->index = (idx + 1) % PREDICT_HISTORY_LEN;

    if (!pred->full) return;

    /* 计算平均速度和方差 */
    float sum_delta_t = 0.0f;
    float sum_delta_d = 0.0f;
    float sum_delta_a = 0.0f;
    int count = 0;

    for (int i = 0; i < PREDICT_HISTORY_LEN - 1; i++) {
        int j = (pred->index - PREDICT_HISTORY_LEN + i + PREDICT_HISTORY_LEN) % PREDICT_HISTORY_LEN;
        int k = (j + 1) % PREDICT_HISTORY_LEN;
        if (pred->time_ms[k] <= pred->time_ms[j]) continue;
        float dt = (pred->time_ms[k] - pred->time_ms[j]) * 0.001f; /* 秒 */
        float dd = pred->dist_history[k] - pred->dist_history[j];
        float da = pred->angle_history[k] - pred->angle_history[j];
        sum_delta_t += dt;
        sum_delta_d += dd;
        sum_delta_a += da;
        count++;
    }

    if (sum_delta_t > 0.0f && count > 0) {
        pred->dist_velocity  = sum_delta_d / sum_delta_t;
        pred->angle_velocity = sum_delta_a / sum_delta_t;
    }

    /* 计算距离和角度的方差 */
    float mean_dist = 0.0f, mean_angle = 0.0f;
    for (int i = 0; i < PREDICT_HISTORY_LEN; i++) {
        mean_dist  += pred->dist_history[i];
        mean_angle += pred->angle_history[i];
    }
    mean_dist  /= PREDICT_HISTORY_LEN;
    mean_angle /= PREDICT_HISTORY_LEN;

    float var_dist = 0.0f, var_angle = 0.0f;
    for (int i = 0; i < PREDICT_HISTORY_LEN; i++) {
        float d1 = pred->dist_history[i] - mean_dist;
        float d2 = pred->angle_history[i] - mean_angle;
        var_dist  += d1 * d1;
        var_angle += d2 * d2;
    }
    pred->dist_variance  = var_dist / PREDICT_HISTORY_LEN;
    pred->angle_variance = var_angle / PREDICT_HISTORY_LEN;

    /* 运动判断：速度超过阈值或方差过大 */
    pred->is_moving = (fabsf(pred->angle_velocity) > 1.0f) || (pred->angle_variance > 2.0f);
}

/*----------------------------------------------------------------------------
 * 获取预判搜索窗口
 *----------------------------------------------------------------------------*/
void TargetPredict_GetSearchWindow(TargetPredict_t *pred, float *center_angle, float *win_width_deg)
{
    if (!pred->full) {
        /* 历史数据不足，返回全扫描角度范围 */
        *center_angle = 0.0f;
        *win_width_deg = PREDICT_WINDOW_MAX_WIDTH_DEG;
        return;
    }

    float last_angle = pred->angle_history[(pred->index - 1 + PREDICT_HISTORY_LEN) % PREDICT_HISTORY_LEN];

    /* 预测0.2秒后的中心角度 */
    float pred_angle = last_angle + pred->angle_velocity * 0.2f;

    /* 窗口宽度 = 基础宽度 + 不确定性补偿 */
    float width = PREDICT_WINDOW_MIN_WIDTH_DEG +
                  sqrtf(pred->angle_variance) * 3.0f +
                  fabsf(pred->angle_velocity) * 0.5f;

    if (width < PREDICT_WINDOW_MIN_WIDTH_DEG) width = PREDICT_WINDOW_MIN_WIDTH_DEG;
    if (width > PREDICT_WINDOW_MAX_WIDTH_DEG) width = PREDICT_WINDOW_MAX_WIDTH_DEG;

    *center_angle = pred_angle;
    *win_width_deg = width;
}

uint8_t TargetPredict_IsMoving(TargetPredict_t *pred)
{
    return pred->is_moving;
}

void TargetPredict_GetVelocity(TargetPredict_t *pred, float *vel_dist, float *vel_angle)
{
    *vel_dist  = pred->dist_velocity;
    *vel_angle = pred->angle_velocity;
}