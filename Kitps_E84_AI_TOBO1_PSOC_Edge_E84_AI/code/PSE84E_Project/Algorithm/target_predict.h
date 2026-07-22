//目标运动预判（防遮挡）
/**
 * @file    target_predict.h
 * @brief   目标运动波动分析与方位预判（防遮挡）
 * 
 * 公开接口：
 *   TargetPredict_Init()            - 初始化预判模块
 *   TargetPredict_UpdateHistory()   - 输入新观测，更新历史轨迹
 *   TargetPredict_GetSearchWindow() - 计算预判搜索窗口（中心角、宽度）
 *   TargetPredict_IsMoving()        - 判断目标是否在运动
 *   TargetPredict_GetVelocity()     - 获取当前距离变化率/角速度
 */

#ifndef __TARGET_PREDICT_H
#define __TARGET_PREDICT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 轨迹历史长度 */
#define PREDICT_HISTORY_LEN    5

/* 搜索窗口约束 */
#define PREDICT_WINDOW_MIN_WIDTH_DEG  5.0f    /* 最小扫描窗口宽度（度） */
#define PREDICT_WINDOW_MAX_WIDTH_DEG  60.0f   /* 最大扫描窗口宽度（度） */

typedef struct {
    float dist_history[PREDICT_HISTORY_LEN];   /* 距离历史 */
    float angle_history[PREDICT_HISTORY_LEN];  /* 角度历史 */
    uint32_t time_ms[PREDICT_HISTORY_LEN];     /* 对应时间戳（ms） */
    uint8_t index;
    uint8_t full;

    float dist_velocity;       /* 当前距离变化率 (cm/s) */
    float angle_velocity;      /* 当前角速度 (°/s) */
    float dist_variance;       /* 距离波动方差 */
    float angle_variance;      /* 角度波动方差 */
    uint8_t is_moving;         /* 是否判断为移动目标 */
} TargetPredict_t;

void  TargetPredict_Init(TargetPredict_t *pred);
void  TargetPredict_UpdateHistory(TargetPredict_t *pred, float dist, float angle, uint32_t time_ms);
void  TargetPredict_GetSearchWindow(TargetPredict_t *pred, float *center_angle, float *win_width_deg);
uint8_t TargetPredict_IsMoving(TargetPredict_t *pred);
void  TargetPredict_GetVelocity(TargetPredict_t *pred, float *vel_dist, float *vel_angle);

#ifdef __cplusplus
}
#endif

#endif /* __TARGET_PREDICT_H */