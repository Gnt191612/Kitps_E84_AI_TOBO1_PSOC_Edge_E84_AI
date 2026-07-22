/**
 * @file    system_XMC8400E.h
 * @brief   PSoC Edge E84 (XMC8400E) 系统声明
 *
 * CMSIS 兼容系统初始化头文件。
 */

#ifndef SYSTEM_XMC8400E_H
#define SYSTEM_XMC8400E_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 系统时钟频率 (Hz)，由 SystemInit 设置 */
extern uint32_t SystemCoreClock;

/**
 * @brief 初始化系统
 *
 * 设置系统时钟、初始化堆栈指针、使能 FPU/Helium（Cortex-M55），
 * 以及必要的系统外设初始化。由启动文件 startup_XMC8400E.S 调用。
 */
void SystemInit(void);

/**
 * @brief 更新 SystemCoreClock 变量
 *
 * 在系统时钟配置更改后调用，以同步 SystemCoreClock 为当前值。
 */
void SystemCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_XMC8400E_H */
