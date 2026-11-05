/**
 * @file    closed_loop.h
 * @brief   目标跟踪自闭环控制
 *
 * 跟踪命令只在分配/接力/复锁时由调度器下发一次。
 * 本模块只做链路看门狗：已跟踪目标的主ESP32长时间无回传时，
 * 允许调度器限频重发一次跟踪命令（如ESP32复位后恢复）。
 * 不得周期性重发：ESP32每收到一次CMD_TRACK都会复锁并重置跟踪器。
 * 重发后仍无回传时，调度器把目标改派给另一块空闲板（事件 FAILOVER）。
 */
#ifndef __CLOSED_LOOP_H
#define __CLOSED_LOOP_H
#include "target_list.h"
#ifdef __cplusplus
extern "C" {
#endif

#define CLOSED_LOOP_LINK_TIMEOUT_MS   1000U  /* 主ESP32无回传超过该时间视为链路中断 */
#define CLOSED_LOOP_RESEND_MIN_MS     2000U  /* 两次恢复性重发的最小间隔 */
#define CLOSED_LOOP_FAILOVER_RESENDS  1U     /* 重发这么多次仍无回传就改派，无回传后约 3~4 s */

void ClosedLoop_Init(void);
/* 返回1：调度器应向 t->esp_assigned 重发一次跟踪命令 */
uint8_t ClosedLoop_NeedsResend(const TrackedTarget_t *t,
                               uint32_t since_rx_ms, uint32_t since_resend_ms);
#ifdef __cplusplus
}
#endif
#endif
