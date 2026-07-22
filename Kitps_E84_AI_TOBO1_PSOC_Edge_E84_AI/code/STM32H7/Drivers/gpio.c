/**
 * @file    gpio.c
 * @brief   GPIO操作实现（依赖CubeMX生成的引脚宏）
 */

#include "gpio.h"
#include "main.h"

void GPIO_SetLED(uint8_t state)
{
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void GPIO_ToggleLED(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

void GPIO_Reset84E(void)
{
    HAL_GPIO_WritePin(RST84E_GPIO_Port, RST84E_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RST84E_GPIO_Port, RST84E_Pin, GPIO_PIN_SET);
}