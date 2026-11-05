/**
 * @file    relock_logic.h
 * @brief   目标跟丢后复锁逻辑（含运动趋势预测 + 资源释放决策）
 *
 * 复锁流程：
 *   1. 使用历史轨迹推算目标当前大致位置，填入 RelockPlan_t
 *   2. 调度器按计划经识别队列请求 84E（先转云台再识别），
 *      并只让该目标的主ESP32（或空闲板）转向预测方位复锁
 *   3. 若连续 RELOCK_MAX_RETRIES 次失败 → 判定目标已超出范围，
 *      返回 RELOCK_RELEASE 通知调度器释放该 ESP32 进程
 *
 * 本模块不直接发送任何命令，避免绕过调度器的 84E 忙闲与 ESP32 占用管理。
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

/* 方位角限幅（相对正前方，°），对应 270° 云台 0°~270° */
#define RELOCK_BEARING_LIMIT 135.0f

/* 一次复锁尝试的执行计划，由调度器执行 */
typedef struct {
    float pred_angle;    /* 预测方位角（相对正前方，°） */
    float pred_dist;     /* 预测距离（cm） */
    float scan_width;    /* 84E 扫描窗口宽度（°） */
    float dist_margin;   /* 84E 距离窗口半宽（cm） */
} RelockPlan_t;

void RelockLogic_Init(void);
/* 返回 RELOCK_RETRY 时 plan 有效；返回 RELOCK_RELEASE 时应释放进程 */
int  RelockLogic_Start(TrackedTarget_t *t, RelockPlan_t *plan);
void RelockLogic_RecordPosition(uint8_t target_id, float angle_deg, float dist_cm);
void RelockLogic_ResetRetries(uint8_t target_id);
/* 换了观测板后方位的基准变了，旧历史不能再用来外推 */
void RelockLogic_ResetHistory(uint8_t target_id, float angle_deg, float dist_cm);

#ifdef __cplusplus
}
#endif

#endif /* __RELOCK_H */
