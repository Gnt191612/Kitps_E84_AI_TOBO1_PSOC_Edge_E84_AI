/**
 * @file    spi.h
 * @brief   SPI通信接口（雷达预留）
 *
 * 公开接口：
 *   SPI_Init(void)                         - 初始化（由CubeMX调用）
 *   SPI_Transmit(uint8_t *data, uint16_t len)   - 发送数据
 *   SPI_Receive(uint8_t *buf, uint16_t len)     - 接收数据
 *   SPI_TransmitReceive(uint8_t *tx, uint8_t *rx, uint16_t len) - 全双工传输
 */

#ifndef __SPI_H
#define __SPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SPI_Init(void);
void SPI_Transmit(uint8_t *data, uint16_t len);
void SPI_Receive(uint8_t *buf, uint16_t len);
void SPI_TransmitReceive(uint8_t *tx, uint8_t *rx, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_H */