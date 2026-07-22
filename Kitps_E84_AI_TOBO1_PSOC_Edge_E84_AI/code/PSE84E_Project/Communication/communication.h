/**
 * @file    communication.h
 * @brief   与 STM32H7 的通信层（基于位操 UART + GPIO 中断）
 *
 * 硬件：P21 端口
 *   - P21_2 → BB UART TX (84E→H7)
 *   - P21_3 → BB UART RX (H7→84E)，下降沿中断检测起始位
 *   - P21_4 (INT2) → 事件通知
 *   - P21_5 (INT3) → 事件通知
 *
 * 接口：
 *   Comm_Init()                     - 初始化 BB UART + 中断线
 *   Comm_SendByte(uint8_t byte)    - 发送单字节
 *   Comm_SendData(data, len)       - 发送数据块
 *   Comm_SetRxCallback(callback)   - 注册接收回调
 */

#ifndef __COMMUNICATION_H
#define __COMMUNICATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Comm_Init(void);
void Comm_SendByte(uint8_t byte);
void Comm_SendData(uint8_t *data, uint16_t len);
void Comm_SetRxCallback(void (*callback)(uint8_t byte));
void Comm_IRQHandler(void);

/* ---- J5 中断线特定接口 ---- */

/**
 * @brief 初始化 J5 四路 GPIO 中断
 * 注册各个中断回调后调用此函数
 */
void Comm_InitInterrupts(void);

/**
 * @brief 注册 SERIAL_INT0 中断回调（雷达扫描完成）
 */
void Comm_SetInt0Callback(void (*callback)(void));

/**
 * @brief 注册 SERIAL_INT1 中断回调（ESP32 跟踪事件）
 */
void Comm_SetInt1Callback(void (*callback)(void));

/**
 * @brief 注册 SERIAL_INT2 中断回调（H7 命令握手）
 */
void Comm_SetInt2Callback(void (*callback)(void));

/**
 * @brief 注册 SERIAL_INT3 中断回调（多目标检测）
 */
void Comm_SetInt3Callback(void (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif /* __COMMUNICATION_H */
