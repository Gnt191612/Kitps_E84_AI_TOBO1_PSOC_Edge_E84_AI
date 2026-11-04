/**
 * @file    assist_localize.h
 * @brief   双ESP32互相辅助定位
 *
 * 只负责计算辅助ESP32的观察方位；何时启动/停止辅助、下发命令和
 * 转动云台由调度器负责，且每次辅助只下发一次跟踪命令。
 */
#ifndef __ASSIST_LOCALIZE_H
#define __ASSIST_LOCALIZE_H
#include "target_list.h"
#ifdef __cplusplus
extern "C" {
#endif
void AssistLocalize_Init(void);
/* 返回辅助ESP32云台应指向的方位角（相对正前方，°） */
float AssistLocalize_ComputeBearing(const TrackedTarget_t *t);
#ifdef __cplusplus
}
#endif
#endif
