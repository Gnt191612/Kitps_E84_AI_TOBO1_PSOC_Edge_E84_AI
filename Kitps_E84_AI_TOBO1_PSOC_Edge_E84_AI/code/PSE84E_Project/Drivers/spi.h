/**
 * @file    spi.h
 * @brief   SPI 驱动封装（基于 Infineon PDL Cy_SCB_SPI API）
 *
 * PSoC Edge E84 的 SPI 由 SCB (Serial Communication Block) 实现，
 * 同一 SCB 可在 UART/SPI/I2C 模式间切换。
 *
 * 接口：
 *   SPI_Init()         - 初始化 SPI（Master 模式）
 *   SPI_Transfer()     - 全双工传输
 *   SPI_Transmit()     - 仅发送
 *   SPI_Receive()      - 仅接收
 */

#ifndef __SPI_H
#define __SPI_H

#include <stdint.h>
#include "cy_pdl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SPI Master
 * @param scb    SCB 指针
 * @param freq   SPI 时钟频率（Hz）
 * @return 0=成功, -1=失败
 */
int SPI_Init(CySCB_Type *scb, uint32_t freq);

/**
 * @brief 全双工传输
 * @param scb    SCB 指针
 * @param txBuf  发送数据
 * @param rxBuf  接收数据（可为 NULL）
 * @param len    传输字节数
 * @return 0=成功, -1=失败
 */
int SPI_Transfer(CySCB_Type *scb, const uint8_t *txBuf, uint8_t *rxBuf, uint16_t len);

/**
 * @brief 仅发送
 * @param scb  SCB 指针
 * @param data 数据
 * @param len  长度
 * @return 0=成功
 */
int SPI_Transmit(CySCB_Type *scb, const uint8_t *data, uint16_t len);

/**
 * @brief 仅接收
 * @param scb  SCB 指针
 * @param data 接收缓冲区
 * @param len  接收字节数
 * @return 0=成功
 */
int SPI_Receive(CySCB_Type *scb, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_H */
