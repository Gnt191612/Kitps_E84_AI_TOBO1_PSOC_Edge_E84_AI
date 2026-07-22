/**
 * @file    assist_localize.h
 * @brief   双ESP32互相辅助定位
 */
#ifndef __ASSIST_LOCALIZE_H
#define __ASSIST_LOCALIZE_H
#include "target_list.h"
#ifdef __cplusplus
extern "C" {
#endif
void AssistLocalize_Init(void);
void AssistLocalize_Execute(TrackedTarget_t *t);
#ifdef __cplusplus
}
#endif
#endif