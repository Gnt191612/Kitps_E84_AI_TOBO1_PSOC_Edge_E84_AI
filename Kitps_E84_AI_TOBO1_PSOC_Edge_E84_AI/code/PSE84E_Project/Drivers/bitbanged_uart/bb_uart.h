/**
 * @file bb_uart.h
 * @brief 位操 UART (Bit-banged UART) — P21_2(TX), P21_3(RX) @ 115200 baud
 *
 * 由于 PSE84E 硬件上仅有 P21 引脚可用（接有排针），
 * 原 SCB0 UART (P0.0/P0.1) 不可用，改用位操 UART 在 P21_2/P21_3 上
 * 模拟 115200 波特率异步串行通信。
 *
 * 接线：
 *   PSE84E P21_2 (TX) → H7 PB7 (USART1_RX)
 *   PSE84E P21_3 (RX) → H7 PB6 (USART1_TX)
 *
 * H7 端仍使用硬件 USART1，完全不受影响。
 *
 * 时序：115200 baud → 1 bit = 8.68μs (160MHz 下 ~1389 周期)
 */

#ifndef BB_UART_H
#define BB_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/* 引脚定义                                                                  */
/* ======================================================================== */
#define BB_TX_PORT          GPIO_PRT21
#define BB_TX_PIN           2U          /* P21_2 — 84E→H7 发送 */
#define BB_RX_PORT          GPIO_PRT21
#define BB_RX_PIN           3U          /* P21_3 — H7→84E 接收 */

/* ======================================================================== */
/* 波特率: 115200 (与 H7 USART1 一致)                                       */
/* ======================================================================== */
#define BB_BAUD             115200
#define BB_BIT_US           8           /* ~8.68μs, 取整 8μs 可容忍 */

/* 接收忙等待超时 */
#define BB_RX_TIMEOUT_US    2000        /* 等待一字节最坏 ~87μs，取 2ms 安全 */

/* ======================================================================== */
/* 接收环形缓冲区                                                             */
/* ======================================================================== */
#define BB_RX_BUF_SIZE      256         /* 接收缓冲区大小 */

/* ======================================================================== */
/* API                                                                       */
/* ======================================================================== */

/**
 * @brief 初始化位操 UART
 * @param rx_callback 收到完整字节时的回调函数
 *
 * 初始化内容：
 *   - TX (P21_2) = 推挽输出，默认高电平（空闲态）
 *   - RX (P21_3) = 输入，下降沿中断检测起始位
 *   - 注册 GPIO P21_3 中断 → BB_UART_RxISR
 */
void BB_UART_Init(void (*rx_callback)(uint8_t byte));

/**
 * @brief 发送一个字节（阻塞，LSB first）
 */
void BB_UART_SendByte(uint8_t byte);

/**
 * @brief 发送多个字节
 */
void BB_UART_SendData(const uint8_t *data, uint16_t len);

/**
 * @brief 接收中断 ISR（由 GPIO 中断服务例程调用）
 */
void BB_UART_RxISR(void);

/**
 * @brief 获取接收回调（供 communication.c 注册用）
 */
void (*BB_UART_GetRxCallback(void))(uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* BB_UART_H */
