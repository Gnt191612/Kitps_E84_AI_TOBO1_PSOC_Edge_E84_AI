/**
 * @file uart.h
 * @brief ESP-IDF UART 驱动封装
 *
 * 接口: UART_Init, UART_Send, UART_SetRxCallback
 * 使用 ESP-IDF v5.x API (uart_driver_install, uart_config_t)
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>
#include "hal/uart_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ──── UART 接收回调 ──── */
typedef void (*UART_RxCallback_t)(uint8_t data);

/* ──── 公共接口 ──── */

/**
 * @brief 初始化 UART
 * @param uart_num  UART 端口号 (UART_NUM_0, UART_NUM_1, UART_NUM_2)
 * @param baud      波特率 (默认 115200)
 * @param tx_pin    TX 引脚
 * @param rx_pin    RX 引脚
 * @return 0=成功, -1=失败
 */
int UART_Init(uart_port_t uart_num, uint32_t baud, int tx_pin, int rx_pin);

/**
 * @brief 发送数据
 * @param uart_num UART 端口号
 * @param data     数据指针
 * @param len      数据长度
 * @return 实际发送字节数
 */
int UART_Send(uart_port_t uart_num, const uint8_t *data, uint16_t len);

/**
 * @brief 设置接收回调 (中断接收模式)
 * @param uart_num UART 端口号
 * @param cb       回调函数 (每收到一字节调用)
 */
void UART_SetRxCallback(uart_port_t uart_num, UART_RxCallback_t cb);

/**
 * @brief UART 接收任务 (运行在 Core 0, 轮询接收队列)
 * @param arg 参数
 */
void UART_RxTask(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
