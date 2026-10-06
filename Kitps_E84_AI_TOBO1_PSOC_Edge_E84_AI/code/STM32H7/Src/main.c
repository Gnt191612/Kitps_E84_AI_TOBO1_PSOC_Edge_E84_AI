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
  /* System_Init 统一完成 HAL、时钟和外设初始化，避免重复初始化 PWM。 */
  System_Init();
  Scheduler_Init();
  while (1)
  {
    Scheduler_Run();
  }
}
