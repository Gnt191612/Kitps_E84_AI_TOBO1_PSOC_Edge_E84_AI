/**
 * @file    closed_loop.h
 * @brief   目标跟踪自闭环控制
 */
#ifndef __CLOSED_LOOP_H
#define __CLOSED_LOOP_H
#include "target_list.h"
#ifdef __cplusplus
extern "C" {
#endif
void ClosedLoop_Init(void);
void ClosedLoop_Check(TrackedTarget_t *t);
#ifdef __cplusplus
}
#endif
#endif