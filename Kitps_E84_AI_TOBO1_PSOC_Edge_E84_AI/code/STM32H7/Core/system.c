/**
 * @file    system.c
 * @brief   系统初始化实现：时钟、外设、基础模块、启动UART中断、I2C 与 84E 通信
 *
 * 外设初始化顺序（与 CubeMX 生成的 MX_*_Init 函数对应）：
 *   System_Init:
 *     HAL_Init → SystemClock_Config → MX_GPIO → MX_UART → MX_SPI
 *     → MX_TIM → MX_FDCAN → I2C2_Master → Servo → Gimbal
 *     → Logger → Radar → Protocol → UART_RxIT
 */

#include "system.h"
#include "main.h"
#include "j5_int.h"             /* J5 中断线脉冲控制 */
#include "logger.h"
#include "radar_process.h"
#include "radar_filter.h"
#include "target_detect.h"
#include "uart.h"               /* 提供 UART_StartRxIT(), UART_Send84E */
#include "protocol.h"
#include "cmd_84e.h"
#include "cmd_esp32.h"
#include "servo.h"              /* 雷达旋转舵机 (PA0 → TIM2_CH1) */
#include "gimbal.h"             /* 三路相机云台 (PA1~PB1, 共6路PWM) */
#include "can.h"                /* CAN 初始化（预留） */

/* 初始姿态角度 */
#define INIT_RADAR_ANGLE       90.0f   /* 雷达初始朝正前方 (+Y) */

void System_Init(void)
{
    /* 1. HAL库初始化 */
    HAL_Init();

    /* 2. 系统时钟配置（CubeMX生成） */
    SystemClock_Config();

    /* 3. 外设初始化（CubeMX生成） */
    MX_GPIO_Init();
    MX_UART_Init();
    MX_SPI_Init();
    MX_TIM_Init();

    /* 3.1 J5 中断线初始化 — H7 作为 Master 输出控制 4 路 INT 线 (低电平触发) */
    J5_InitAll();

    /* 3.2 舵机初始化 + 初始姿态设置 */
    Servo_Init();                      /* 雷达旋转舵机 (PA0 → TIM2_CH1) */
    Servo_SetAngle(INIT_RADAR_ANGLE);  /* 雷达朝正前方 */

    Gimbal_Init();                     /* 三路云台归中 (Pan=90°, Tilt=90°) */

    /* 4. 应用层基础模块 */
    Logger_Init();
    Logger_SetLevel(LOG_INFO);
    Logger_Print(LOG_INFO, "STM32H7 RadarMaster System Init...");
    Logger_Print(LOG_INFO, "USART1=84E(PB6/PB7) UART2=ESP32-A UART3=ESP32-B");

    /* 雷达数据处理模块 */
    Radar_Process_Init();
    Radar_Filter_Init();
    TargetDetect_Init();

    /* 启动三路UART中断接收（将字节喂给Protocol层） */
    UART_StartRxIT();

    /* 通信协议栈初始化 */
    Protocol_Init();
    Cmd_84E_Init();
    Cmd_ESP32_Init();

    /* CAN 初始化（预留） */
    CAN_Init();

    Logger_Print(LOG_INFO, "System Init Complete. All servos at initial pose.");
}
