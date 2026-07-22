/**
 * @file    timer.h
 * @brief   定时器接口（提供毫秒计时和延时）
 *
 * 公开接口：
 *   Timer_GetMs(void)            - 返回系统启动以来的毫秒数
 *   Timer_DelayMs(uint32_t ms)   - 阻塞延时
 */

#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t Timer_GetMs(void);
void Timer_DelayMs(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* __TIMER_H */