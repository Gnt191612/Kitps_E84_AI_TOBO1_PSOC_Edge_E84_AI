/**
 * @file system.h
 * @brief 系统初始化与调度
 *
 * GOOUUU ESP32-S3-CAM N16R8（16 MB Flash + 8 MB PSRAM）基础外设与系统函数。
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

/* 当前阶段只启用H7板间UART通信，暂不启动外部WiFi/WebSocket操控端。 */
#define ENABLE_EXTERNAL_CONTROL  0

#ifdef __cplusplus
extern "C" {
#endif

/* ──── UART 端口分配 ──── */
#define UART_CONSOLE    UART_NUM_0    /* 调试串口 (115200) */
#define UART_H7         UART_NUM_1    /* 与 H7 通信串口 (115200 8N1) */

/* ──── GOOUUU ESP32-S3-CAM 引脚分配 ──── */
/* GPIO9/10 已由板载OV2640占用，H7 UART改用排针GPIO1/2。 */
#define PIN_UART1_TX    GPIO_NUM_1
#define PIN_UART1_RX    GPIO_NUM_2

/* ──── 任务栈 ──── */
#define UART_RX_TASK_STACK 2048
#define SCHEDULER_STACK    4096

/* ──── 角色检测引脚 ──── */
/* ESP32-A: GPIO14拉高；ESP32-B: GPIO14拉低或悬空。 */
#define GPIO_ROLE_DETECT    GPIO_NUM_14

/* ──── 系统函数 ──── */

/**
 * @brief 系统初始化
 * - 初始化 UART (115200 8N1)
 * - 初始化 OV2640 摄像头
 * - 初始化 GPIO 和 PWM
 * - 初始化日志系统
 * - 启动 UART 接收任务
 */
void System_Init(void);

/**
 * @brief 调度器运行 (主循环)
 * - 每帧轮转: 接收H7命令 → 采集图像 → 预处理 → ROI提取 → 跟踪计算 → 发送结果
 */
void Scheduler_Run(void);

/**
 * @brief H7 命令回调处理
 */
void System_OnCommand(uint8_t cmd, uint8_t target_id, int32_t param1, int32_t param2);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_H */
