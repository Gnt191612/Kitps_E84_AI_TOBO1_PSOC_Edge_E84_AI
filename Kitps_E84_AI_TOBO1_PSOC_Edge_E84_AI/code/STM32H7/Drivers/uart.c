/**
 * @file    uart.c
 * @brief   UART收发实现（中断接收 + 协议对接）
 *
 * 硬件映射（与 wiring.md 一致）：
 *   huart1 -> USART1 (PB6/PB7) — 连 PSE84E SCB0 (P0.0/P0.1)
 *   huart2 -> USART2 (PD5/PD6) — ESP32-A
 *   huart3 -> USART3 (PD8/PD9) — ESP32-B
 *
 * 84E 通过 USART1 直连，TX⇄RX 交叉，115200bps.
 * J5 SDA/SCL 不连接，仅使用四路 INT 线作为事件通知。
 */

#include "uart.h"
#include "main.h"

#include "protocol.h"             // Protocol_Feed84EByte / Protocol_FeedESP32Byte
#include "logger.h"

/* 单字节接收缓冲 */
static uint8_t rx1_byte;   /* USART1 — PSE84E 数据 */
static uint8_t rx2_byte;   /* USART2 — ESP32-A */
static uint8_t rx3_byte;   /* USART3 — ESP32-B */

/* 启动中断接收（在 System_Init 中调用） */
void UART_StartRxIT(void)
{
    HAL_UART_Receive_IT(&huart1, &rx1_byte, 1);   /* PSE84E (SCB0 UART) */
    HAL_UART_Receive_IT(&huart2, &rx2_byte, 1);   /* ESP32-A */
    HAL_UART_Receive_IT(&huart3, &rx3_byte, 1);   /* ESP32-B */
}

/* 发送实现 — 84E 通过 USART1 (PB6/PB7, 115200) */
void UART_Send84E(uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, data, len, 100);
}

void UART_SendESP32(uint8_t esp_id, uint8_t *data, uint16_t len)
{
    if (esp_id == 0) {
        HAL_UART_Transmit(&huart2, data, len, 100);
    } else {
        HAL_UART_Transmit(&huart3, data, len, 100);
    }
}

/* 中断回调：字节直接喂给协议层 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        Protocol_Feed84EByte(rx1_byte);
        HAL_UART_Receive_IT(&huart1, &rx1_byte, 1);
    }
    else if (huart->Instance == USART2) {
        Protocol_FeedESP32Byte(rx2_byte, 0);
        HAL_UART_Receive_IT(&huart2, &rx2_byte, 1);
    }
    else if (huart->Instance == USART3) {
        Protocol_FeedESP32Byte(rx3_byte, 1);
        HAL_UART_Receive_IT(&huart3, &rx3_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        HAL_UART_Receive_IT(&huart1, &rx1_byte, 1);
    }
    else if (huart->Instance == USART2) {
        HAL_UART_Receive_IT(&huart2, &rx2_byte, 1);
    }
    else if (huart->Instance == USART3) {
        HAL_UART_Receive_IT(&huart3, &rx3_byte, 1);
    }
}

/**
 * @brief 从 84E 通过 USART1 读取数据
 * @deprecated 已由中断接收替代，保留兼容
 * @return 0（数据由中断直接喂给协议层）
 */
int UART_Read84E_I2C(uint8_t *data, uint16_t len)
{
    (void)data; (void)len;
    return 0;
}
