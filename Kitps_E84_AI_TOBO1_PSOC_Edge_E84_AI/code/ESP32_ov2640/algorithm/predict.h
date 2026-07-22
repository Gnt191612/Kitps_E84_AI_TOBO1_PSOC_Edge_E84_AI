/**
 * @file predict.h
 * @brief 目标运动预测 (简单匀速/加速度模型)
 *
 * 接口: Predict_Init, Predict_Update
 */

#ifndef PREDICT_H
#define PREDICT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 预测器结构 ──── */
typedef struct {
    float pos_x;        /* 当前位置 X */
    float pos_y;        /* 当前位置 Y */
    float vel_x;        /* X 方向速度 */
    float vel_y;        /* Y 方向速度 */
    float prev_x;       /* 上一帧 X */
    float prev_y;       /* 上一帧 Y */
    uint32_t prev_time; /* 上一帧时间戳 (ms) */
    int initialized;
} Predictor_t;

/* ──── 公共接口 ──── */

/**
 * @brief 初始化预测器
 * @param pred 预测器指针
 */
void Predict_Init(Predictor_t *pred);

/**
 * @brief 更新预测器 (每帧调用)
 * @param pred   预测器指针
 * @param pos_x  当前测量 X
 * @param pos_y  当前测量 Y
 * @param dt     距上次更新的时间 (秒)
 * @param pred_x 输出预测下一帧 X
 * @param pred_y 输出预测下一帧 Y
 */
void Predict_Update(Predictor_t *pred, float pos_x, float pos_y, float dt,
                    float *pred_x, float *pred_y);

/**
 * @brief 重置预测器
 * @param pred 预测器指针
 */
void Predict_Reset(Predictor_t *pred);

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_H */
