/**
 * @file    main.h
 * @brief   全局外设句柄声明 + 引脚宏定义（委托 Inc/main.h 提供引脚定义）
 *
 * 注意：CubeMX 生成的 Inc/main.h 包含引脚宏定义。
 *       本文件通过 #include 引入后者，
 *       再补充外设句柄 extern 和函数声明，
 *       避免重复定义冲突。
 */

#ifndef __CORE_MAIN_H
#define __CORE_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* 引入 CubeMX 生成的引脚宏定义（Inc/main.h） */
#include "../Inc/main.h"

/* ──── 外设句柄 ──── */
/* 这些由 CubeMX 生成的 hal 文件（stm32h7xx_hal_msp.c）中定义 */
extern UART_HandleTypeDef huart1;   // 与84E通信
extern UART_HandleTypeDef huart2;   // 与ESP32_A通信
extern UART_HandleTypeDef huart3;   // 与ESP32_B通信
// extern SPI_HandleTypeDef  hspi1;    // 雷达SPI（本例保留，未启用）
extern TIM_HandleTypeDef  htim2;    // 云台舵机 PWM (PA0 → TIM2_CH1)
extern TIM_HandleTypeDef  htim3;    // 超声波捕获 (PC6 → TIM3_CH1)
extern TIM_HandleTypeDef  htim6;    // 调度器时基

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_UART_Init(void);
void MX_TIM_Init(void);
void MX_SPI_Init(void);

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __CORE_MAIN_H */
