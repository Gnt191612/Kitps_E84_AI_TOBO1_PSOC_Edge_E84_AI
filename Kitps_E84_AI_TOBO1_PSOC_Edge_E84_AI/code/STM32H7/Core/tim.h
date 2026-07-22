/**
 * @file    tim.h
 * @brief   CubeMX 定时器初始化声明（存根）
 *
 * 实际初始化代码由 CubeMX 生成在 stm32h7xx_hal_msp.c 中。
 * 本头文件提供 extern 声明供自定义驱动调用。
 */
#ifndef __TIM_H
#define __TIM_H

#include "main.h"

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim6;

void MX_TIM2_Init(void);
void MX_TIM3_Init(void);
void MX_TIM6_Init(void);

#endif /* __TIM_H */
