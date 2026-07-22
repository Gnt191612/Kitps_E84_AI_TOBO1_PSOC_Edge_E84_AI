/**
 * @file    target_state.h
 * @brief   目标状态机
 *
 * 定义目标跟踪状态枚举以及状态更新接口。
 * 状态字段含义（存储于 TrackedTarget_t.state）：
 *   0 = TARGET_STATE_IDLE     – 空闲（未被跟踪）
 *   1 = TARGET_STATE_SCANNING – 正在搜索/扫描
 *   2 = TARGET_STATE_TRACKED  – 正在被跟踪
 *   3 = TARGET_STATE_LOST     – 已丢失
 *
 * 公开接口：
 *   TargetState_Update() - 根据新信息更新目标状态
 */

#ifndef __TARGET_STATE_H
#define __TARGET_STATE_H

#include "target_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 目标状态常量 */
#define TARGET_STATE_IDLE       0
#define TARGET_STATE_SCANNING   1
#define TARGET_STATE_TRACKED    2
#define TARGET_STATE_LOST       3

void TargetState_Update(TargetList_t *list, uint8_t id, uint8_t new_state);

#ifdef __cplusplus
}
#endif

#endif /* __TARGET_STATE_H */
