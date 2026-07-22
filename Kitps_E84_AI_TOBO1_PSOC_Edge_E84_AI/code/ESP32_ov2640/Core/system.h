/**
 * @file system.h
 * @brief 系统初始化与调度
 *
 * 声明基础外设句柄和系统函数。
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── UART 端口分配 ──── */
#define UART_CONSOLE    UART_NUM_0    /* 调试串口 (115200) */
#define UART_H7         UART_NUM_1    /* 与 H7 通信串口 (115200 8N1) */

/* ──── 引脚分配 (需根据实际硬件调整) ──── */
#define PIN_UART0_TX    GPIO_NUM_1
#define PIN_UART0_RX    GPIO_NUM_3
#define PIN_UART1_TX    GPIO_NUM_10
#define PIN_UART1_RX    GPIO_NUM_9

/* ──── 任务栈 ──── */
#define UART_RX_TASK_STACK 2048
#define SCHEDULER_STACK    4096

/* ──── 角色检测引脚 ──── */
/* ESP32-A (WS服务器): GPIO4 拉高 (接3.3V) */
/* ESP32-B (WS客户端): GPIO4 拉低 (接GND或悬空) */
#define GPIO_ROLE_DETECT    GPIO_NUM_4

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
