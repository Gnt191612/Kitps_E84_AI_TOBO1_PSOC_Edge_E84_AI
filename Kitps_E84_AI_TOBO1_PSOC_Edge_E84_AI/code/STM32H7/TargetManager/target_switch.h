/**
 * @file    target_switch.h
 * @brief   第三个目标出现时启动局域网切换
 *
 * 公开接口：
 *   TargetSwitch_Execute() - 挑选一个目标发送到上位机
 */

#ifndef __TARGET_SWITCH_H
#define __TARGET_SWITCH_H

#include "target_list.h"

void TargetSwitch_Execute(TargetList_t *list);

#endif /* __TARGET_SWITCH_H */