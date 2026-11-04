/**
 * @file    target_switch.h
 * @brief   第三个目标出现时启动局域网切换
 *
 * 公开接口：
 *   TargetSwitch_Execute() - 把没有空闲 ESP32 可跟踪的目标上报上位机并语音播报
 */

#ifndef __TARGET_SWITCH_H
#define __TARGET_SWITCH_H

#include "target_list.h"

void TargetSwitch_Execute(TargetList_t *list, uint8_t unassigned_id);

#endif /* __TARGET_SWITCH_H */