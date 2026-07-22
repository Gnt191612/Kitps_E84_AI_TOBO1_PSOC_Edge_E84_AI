/**
 * @file    timer.c
 * @brief   基于 HAL_GetTick 实现
 */

#include "timer.h"
#include "main.h"

uint32_t Timer_GetMs(void)
{
    return HAL_GetTick();
}

void Timer_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}