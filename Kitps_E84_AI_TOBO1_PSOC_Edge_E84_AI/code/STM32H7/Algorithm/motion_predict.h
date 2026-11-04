/**
 * @file    motion_predict.h
 * @brief   基于历史观测的目标方位预判（开题创新点二“波动预判扫描”的 H7 实现）
 *
 * 每个目标一组卡尔曼滤波（距离 + 方位，解耦），并保留最近若干帧方位
 * 计算波动（标准差）。调度器在请求 84E 识别 / 复锁时用它：
 *   - 预判方位 = 卡尔曼外推到“云台转到位 + 推理完成”时刻的方位
 *   - 扫描窗口随方位波动放宽，减少遮挡/机动导致的漏识别
 *
 * 公开接口：
 *   MotionPredict_Init()   - 清空全部目标
 *   MotionPredict_Update() - 输入一次观测（调用方保证间隔约 MOTION_PREDICT_DT_S）
 *   MotionPredict_Get()    - 外推 horizon_s 秒后的方位/距离和方位波动
 *   MotionPredict_Reset()  - 目标释放时清除
 */
#ifndef __MOTION_PREDICT_H
#define __MOTION_PREDICT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_PREDICT_DT_S       0.1f   /* 期望的观测间隔 */
#define MOTION_PREDICT_MIN_SAMPLES  3U   /* 少于该样本数不做外推 */

void    MotionPredict_Init(void);
void    MotionPredict_Update(uint8_t target_id, float bearing_deg, float dist_cm);
/* 返回 1 = 预测有效 */
uint8_t MotionPredict_Get(uint8_t target_id, float horizon_s,
                          float *bearing_deg, float *dist_cm, float *spread_deg);
void    MotionPredict_Reset(uint8_t target_id);

#ifdef __cplusplus
}
#endif

#endif /* __MOTION_PREDICT_H */
