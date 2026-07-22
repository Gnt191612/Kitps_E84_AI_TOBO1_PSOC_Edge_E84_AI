/**
 * @file    relock_logic.h
 * @brief   目标跟丢后复锁逻辑（含运动趋势预测 + 资源释放决策）
 *
 * 复锁流程：
 *   1. 使用卡尔曼预测/历史轨迹推算目标当前大致位置
 *   2. 命令 84E 和两个 ESP32 在预测区域扫描
 *   3. 若复锁成功 → 返回 X_SUCCESS
 *   4. 若连续 RELOCK_MAX_RETRIES 次失败 → 判定目标已超出范围，
 *      返回 X_RELEASE 通知调度器释放该 ESP32 进程
 */
#ifndef __RELOCK_LOGIC_H
#define __RELOCK_LOGIC_H

#include "target_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 复锁结果 */
#define RELOCK_SUCCESS   0   /* 成功复锁 */
#define RELOCK_RETRY    -1   /* 继续重试 */
#define RELOCK_RELEASE  -2   /* 目标超出范围，释放进程 */

/* 最大重试次数（约 5×300ms ≈ 1.5s 重锁窗口） */
#define RELOCK_MAX_RETRIES   5

void RelockLogic_Init(void);
int  RelockLogic_Start(TrackedTarget_t *t);
void RelockLogic_RecordPosition(uint8_t target_id, float angle_deg, float dist_cm);
void RelockLogic_ResetRetries(uint8_t target_id);

#ifdef __cplusplus
}
#endif

#endif /* __RELOCK_H */
