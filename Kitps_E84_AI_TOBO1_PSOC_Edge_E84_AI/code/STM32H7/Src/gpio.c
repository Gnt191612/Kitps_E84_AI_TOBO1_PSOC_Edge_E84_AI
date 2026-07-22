/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
     PA4   ------> SPI1_NSS
     PA5   ------> SPI1_SCK
     PA6   ------> SPI1_MISO
     PA7   ------> SPI1_MOSI
     
     注意：舵机 PWM 引脚(PA0~PA3/PC7/PB0~PB1)的 AF 配置
     在 MX_GPIO_Init 函数末尾手动添加（USER CODE BEGIN 2 段）。
     若 CubeMX 重新生成，需要确认该段保留。
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0|RST84E_Pin|LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC0 RST84E_Pin LED_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|RST84E_Pin|LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_NSS_Pin SPI1_SCK_Pin SPI1_MISO_Pin SPI1_MOSI_Pin */
  GPIO_InitStruct.Pin = SPI1_NSS_Pin|SPI1_SCK_Pin|SPI1_MISO_Pin|SPI1_MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*AnalogSwitch Config */
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC2, SYSCFG_SWITCH_PC2_CLOSE);

/* USER CODE BEGIN 2 */

  /* ── 舵机 PWM 引脚复用配置 ──
   * 如果 CubeMX 重新生成后丢失，请将以下代码放回此处。
   * 
   * 引脚映射：
   *   PA0 → TIM2_CH1 (AF1)  雷达旋转舵机
   *   PA1 → TIM2_CH2 (AF1)  CAM0 Pan
   *   PA2 → TIM2_CH3 (AF1)  CAM0 Tilt
   *   PA3 → TIM2_CH4 (AF1)  CAM1 Pan
   *   PC7 → TIM3_CH2 (AF1)  CAM1 Tilt
   *   PB0 → TIM3_CH3 (AF1)  CAM2 Pan
   *   PB1 → TIM3_CH4 (AF1)  CAM2 Tilt
   */

  /* 雷达旋转舵机: PA0 → TIM2_CH1 (AF1) */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* CAM0 Pan(PA1→TIM2_CH2) + Tilt(PA2→TIM2_CH3), 同为 AF1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* CAM1 Pan: PA3 → TIM2_CH4 (AF1) */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* CAM1 Tilt: PC7 → TIM3_CH2 (AF2) */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* CAM2 Pan(PB0→TIM3_CH3) + Tilt(PB1→TIM3_CH4), 同为 AF2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE END 2 */

}

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */
