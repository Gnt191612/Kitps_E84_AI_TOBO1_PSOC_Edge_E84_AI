/**
 * @file    main.h
 * @brief   全局宏定义、外设句柄声明
 *
 * 说明：
 *   - 使用 XMC8400E 的 PDL (mtb-pdl-cat1)
 *   - 与 H7 通信：SCB0 UART (P0.0/P0.1, 115200bps) + J5 四路 GPIO 中断
 *     （J5 SDA/SCL 未连接）
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/* NVIC 宏操作（必须在 cy_pdl.h 之前定义，供 XMC8400E.h 使用）               */
/* 注意：因 uint32_t/IRQn_Type 在 PDL 中定义，此处用宏而非内联函数           */
/* ======================================================================== */
#ifndef NVIC_SETPRIORITY_DEFINED
#define NVIC_SETPRIORITY_DEFINED
/* NVIC set priority: reg = 0xE000E400 + (irq>>2)*4, shift = (irq&3)*8 */
#define NVIC_SetPriority(irq_n, prio) \
    do { \
        volatile uint32_t *_ipr = (volatile uint32_t *)0xE000E400UL; \
        uint32_t _idx = (uint32_t)(irq_n) >> 2; \
        uint32_t _sh = ((uint32_t)(irq_n) & 3U) << 3U; \
        _ipr[_idx] = (_ipr[_idx] & ~(0xFFUL << _sh)) | (((uint32_t)(prio) & 0xFFUL) << _sh); \
    } while(0)
#endif

#ifndef NVIC_ENABLEIRQ_DEFINED
#define NVIC_ENABLEIRQ_DEFINED
#define NVIC_EnableIRQ(irq_n) \
    do { \
        volatile uint32_t *_iser = (volatile uint32_t *)0xE000E100UL; \
        _iser[0] = 1UL << ((uint32_t)(irq_n) & 0x1FUL); \
    } while(0)
#endif

/* ======================================================================== */
/* PDL 头文件                                                                */
/* ======================================================================== */
#include "cy_pdl.h"
#include "XMC8400E.h"

/* 系统初始化函数 */
void System_Init(void);
void System_Start(void);

/* 串口句柄（由 system.c 定义，供各模块 extern 使用） */
extern cy_stc_scb_uart_context_t g_scb_uart_context;

/* HAL 库初始化封装 */
void HAL_Init(void);
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_UART_Init(void);
void MX_TIM_Init(void);

/* 系统滴答（由 SysTick 中断更新） */
extern volatile uint32_t g_sysTickMs;

/* 毫秒级计时（SysTick 中断驱动） */
uint32_t HAL_GetTick(void);

/* 微秒级计时（DWT CYCCNT 驱动） */
uint32_t GetMicros(void);

/* 系统 core clock */
extern uint32_t SystemCoreClock;

/* DWT 寄存器 */
#ifndef DWT_CYCCNT
#define DWT_CYCCNT    (*((volatile uint32_t *)0xE0001004UL))
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
