/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c (CubeMX 入口版本)
  * @brief          : 主程序入口
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "scheduler.h"
#include "protocol.h"
#include "system.h"
#include "gimbal.h"
/* USER CODE END Includes */

void SystemClock_Config(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_UART_Init();
  MX_SPI_Init();
  MX_TIM_Init();

  /* 应用层初始化 */
  System_Init();

  /* 寄存器级强制 PA1/PA3 为 TIM2 复用输出 */
  GPIOA->MODER = (GPIOA->MODER & ~(3<<2)) | (2<<2);  /* PA1=AF */
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xF<<4)) | (1<<4); /* PA1=AF1*/
  GPIOA->MODER = (GPIOA->MODER & ~(3<<6)) | (2<<6);  /* PA3=AF */
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xF<<12)) | (1<<12); /* PA3=AF1*/

  /* 在调度器启动前，强制使能 TIM2 全部4路 + TIM3 3路输出 */
  /* 防止任何初始化顺序导致通道被意外关闭 */
  TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
  TIM3->CCER |= TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

  /* 调度器初始化 */
  Scheduler_Init();

  /* 上电自检：所有云台舵机依次扫过 0°→180°→90°，验证硬件接线 */
  for (int g = 0; g < 3; g++) {
      /* Pan 慢扫 */
      for (float a = 90; a <= 150; a += 5) { Gimbal_SetPan((GimbalId_t)g, a); HAL_Delay(80); }
      for (float a = 150; a >= 30; a -= 5) { Gimbal_SetPan((GimbalId_t)g, a); HAL_Delay(80); }
      for (float a = 30; a <= 90; a += 5)  { Gimbal_SetPan((GimbalId_t)g, a); HAL_Delay(80); }
      /* Tilt 慢扫 */
      for (float a = 90; a <= 120; a += 3) { Gimbal_SetTilt((GimbalId_t)g, a); HAL_Delay(60); }
      for (float a = 120; a >= 60; a -= 3) { Gimbal_SetTilt((GimbalId_t)g, a); HAL_Delay(60); }
      for (float a = 60; a <= 90; a += 3)  { Gimbal_SetTilt((GimbalId_t)g, a); HAL_Delay(60); }
  }
  /* 自检完毕，归中 */
  Gimbal_CenterAll();

  /* 诊断：每帧强制写入 PA1(TIM2_CH2) 和 PA3(TIM2_CH4) 的 CCR，确保有信号输出 */
  uint32_t test_counter = 0;
  while (1)
  {
    Scheduler_Run();
    
    /* 每帧交替写入不同 CCR 值到横向舵机 */
    test_counter = (test_counter + 1) % 200;
    uint32_t test_ccr;
    if (test_counter < 100)
        test_ccr = 500 + test_counter * 20;  /* 500~2480 慢扫 */
    else
        test_ccr = 2500 - (test_counter - 100) * 20;
    
    TIM2->CCR2 = test_ccr;  /* CAM0 Pan: PA1 */
    TIM2->CCR4 = test_ccr;  /* CAM1 Pan: PA3 */
    HAL_Delay(10);
  }
}
