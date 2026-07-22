/**
 * @file    uart.h
 * @brief   UART 驱动封装（基于 Infineon PDL Cy_SCB_UART API）
 *
 * PSoC Edge E84 的 UART 由 SCB (Serial Communication Block) 实现。
 * SCB0 为 DeepSleep 可唤醒，SCB1~10 为 Active only。
 *
 * 接口：
 *   UART_Init()         - 初始化 UART
 *   UART_Send()         - 发送数据
 *   UART_SetRxCallback  - 设置接收中断回调
 */

#ifndef __UART_H
#define __UART_H

#include <stdint.h>
#include "cy_pdl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 UART（SCB 模式）
 * @param scb       SCB 指针（如 SCB0, SCB1...）
 * @param baud      波特率（如 115200）
 * @param tx_pin    TX 引脚编号
 * @param rx_pin    RX 引脚编号
 * @param port      GPIO 端口指针
 * @return 0=成功, -1=失败
 */
int UART_Init(CySCB_Type *scb, uint32_t baud,
              GPIO_PRT_Type *port, uint32_t tx_pin, uint32_t rx_pin);

/**
 * @brief 发送数据
 * @param scb   SCB 指针
 * @param data  数据缓冲区
 * @param len   长度
 * @return 实际发送字节数
 */
int UART_Send(CySCB_Type *scb, const uint8_t *data, uint16_t len);

/**
 * @brief 设置接收回调（中断接收）
 * @param scb   SCB 指针
 * @param cb    回调函数（在 SCB 中断中调用）
 */
void UART_SetRxCallback(CySCB_Type *scb, void (*cb)(uint8_t byte));

/**
 * @brief UART 中断处理函数（在 SCB IRQHandler 中调用）
 * @param scb   SCB 指针
 */
void UART_IrqHandler(CySCB_Type *scb);

#ifdef __cplusplus
}
#endif

#endif /* __UART_H */
