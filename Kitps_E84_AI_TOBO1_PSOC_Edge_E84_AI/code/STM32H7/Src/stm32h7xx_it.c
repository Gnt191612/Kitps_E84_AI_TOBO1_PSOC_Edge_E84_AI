/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
  *
  * 本文件在 CubeMX 骨架基础上添加了竞赛所需的中断处理程序：
  *   - USART1/2/3 中断 → 字节喂给 Protocol 层（uart.c 回调转发）
  *   - TIM3 中断（溢出/捕获）→ 雷达 HC-SR04 ECHO 测量
  *   - I2C2 事件/错误中断（预留，当前使用轮询模式）
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "uart.h"
#include "radar_driver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern I2C_HandleTypeDef hi2c2;    /* I2C2 Master — J5 ↔ PSE84E */
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers             */
/******************************************************************************/
void NMI_Handler(void)
{
  while (1) { }
}

void HardFault_Handler(void)
{
  while (1) { }
}

void MemManage_Handler(void)
{
  while (1) { }
}

void BusFault_Handler(void)
{
  while (1) { }
}

void UsageFault_Handler(void)
{
  while (1) { }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                   */
/******************************************************************************/

/* ====================================================================== */
/* USART1 中断（原 84E 通信，已弃用，保留接收以防复用） */
/* ====================================================================== */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

/* ====================================================================== */
/* USART2 中断 — ESP32-A 数据接收 */
/* ====================================================================== */
void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
}

/* ====================================================================== */
/* USART3 中断 — ESP32-B 数据接收 */
/* ====================================================================== */
void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart3);
}

/* ====================================================================== */
/* TIM2 中断 — 舵机 PWM (PA0~PA3) — 由 HAL 库管理                     */
/* ====================================================================== */
void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim2);
}

/* ====================================================================== */
/* TIM3 中断 — 雷达 ECHO 输入捕获 (PC6) + 云台 PWM (PC7/PB0/PB1)     */
/* ====================================================================== */
void TIM3_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim3);
}

/* ====================================================================== */
/* I2C2 事件中断 — 当前使用轮询模式，保留用于中断驱动模式切换 */
/* ====================================================================== */
void I2C2_EV_IRQHandler(void)
{
  HAL_I2C_EV_IRQHandler(&hi2c2);
}

void I2C2_ER_IRQHandler(void)
{
  HAL_I2C_ER_IRQHandler(&hi2c2);
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
