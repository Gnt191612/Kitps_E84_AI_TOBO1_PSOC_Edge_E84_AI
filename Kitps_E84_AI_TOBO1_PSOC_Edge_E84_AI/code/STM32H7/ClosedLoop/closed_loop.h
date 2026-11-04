/**
 * @file    closed_loop.h
 * @brief   目标跟踪自闭环控制
 *
 * 跟踪命令只在分配/接力/复锁时由调度器下发一次。
 * 本模块只做链路看门狗：已跟踪目标的主ESP32长时间无回传时，
 * 允许调度器限频重发一次跟踪命令（如ESP32复位后恢复）。
 * 不得周期性重发：ESP32每收到一次CMD_TRACK都会复锁并重置跟踪器。
 */
#ifndef __CLOSED_LOOP_H
#define __CLOSED_LOOP_H
#include "target_list.h"
#ifdef __cplusplus
extern "C" {
#endif

#define CLOSED_LOOP_LINK_TIMEOUT_MS   1000U  /* 主ESP32无回传超过该时间视为链路中断 */
#define CLOSED_LOOP_RESEND_MIN_MS     2000U  /* 两次恢复性重发的最小间隔 */

void ClosedLoop_Init(void);
/* 返回1：调度器应向 t->esp_assigned 重发一次跟踪命令 */
uint8_t ClosedLoop_NeedsResend(const TrackedTarget_t *t,
                               uint32_t since_rx_ms, uint32_t since_resend_ms);
#ifdef __cplusplus
}
#endif
#endif
