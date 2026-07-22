/**
 * @file    uart.h
 * @brief   多路UART底层收发接口
 *
 * 公开接口：
 *   UART_Send84E(uint8_t *data, uint16_t len)        - 向84E发送数据
 *   UART_SendESP32(uint8_t esp_id, uint8_t *data, uint16_t len) - 向指定ESP32发送
 *
 * 注意：接收字节的中断处理已在本文件中实现，并直接转发给Protocol层。
 */

#ifndef __UART_H
#define __UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void UART_Send84E(uint8_t *data, uint16_t len);
void UART_SendESP32(uint8_t esp_id, uint8_t *data, uint16_t len);

/**
 * @brief 从 84E 通过 I2C 读取数据并喂给协议层
 * @param data  接收缓冲区
 * @param len   期望读取字节数
 * @return 实际读取字节数
 */
int UART_Read84E_I2C(uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __UART_H */