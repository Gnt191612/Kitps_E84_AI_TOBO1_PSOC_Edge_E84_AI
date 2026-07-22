/**
 * @file    timer.h
 * @brief   定时器驱动封装（基于 Infineon PDL Cy_TCPWM）
 *
 * PSoC Edge E84 使用 TCPWM (Timer/Counter PWM) 模块。
 *
 * 接口：
 *   Timer_Init()      - 初始化 TCPWM 定时器
 *   Timer_Start()     - 启动定时器
 *   Timer_Stop()      - 停止定时器
 *   Timer_GetTick_us  - 获取微秒级计数值
 */

#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>
#include "cy_pdl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 TCPWM 为定时器模式
 * @param tcpwm   TCPWM 指针（如 TCPWM0）
 * @param group   定时器组号 (0~15)
 * @param period  计数周期（计数器最大值）
 * @param clockHz TCPWM 输入时钟频率 (Hz)
 * @return 0=成功, -1=失败
 */
int Timer_Init(TCPWM_Type *tcpwm, uint32_t group, uint32_t period, uint32_t clockHz);

/**
 * @brief 启动定时器
 * @param tcpwm TCPWM 指针
 * @param group 组号
 */
void Timer_Start(TCPWM_Type *tcpwm, uint32_t group);

/**
 * @brief 停止定时器
 * @param tcpwm TCPWM 指针
 * @param group 组号
 */
void Timer_Stop(TCPWM_Type *tcpwm, uint32_t group);

/**
 * @brief 获取当前计数器值
 * @param tcpwm TCPWM 指针
 * @param group 组号
 * @return 当前计数值
 */
static inline uint32_t Timer_GetCount(TCPWM_Type *tcpwm, uint32_t group)
{
    return Cy_TCPWM_Block_GetCounter(tcpwm, group);
}

#ifdef __cplusplus
}
#endif

#endif /* __TIMER_H */
