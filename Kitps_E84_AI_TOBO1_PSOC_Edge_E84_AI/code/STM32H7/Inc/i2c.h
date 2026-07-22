/**
 * @file    i2c.h
 * @brief   I2C 主机驱动 — 当前未使用，保留以备复用
 *
 * 数据通道已退回 USART1 (PB6/PB7)，I2C 暂不启用。
 * 如需恢复，取消注释以下宏并运行 I2C_Master_Init()。
 *
 * #define ENABLE_I2C_MASTER
 */

#ifndef __I2C_H7_H
#define __I2C_H7_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_I2C_MASTER

#define PSE84E_I2C_ADDR     0x42U
#define I2C_TIMEOUT_MS      100U

int I2C_Master_Init(void);
int I2C_Write(uint8_t *data, uint16_t len);
int I2C_Read(uint8_t *data, uint16_t len);
int I2C_WriteRead(uint8_t *tx_data, uint16_t tx_len,
                  uint8_t *rx_data, uint16_t rx_len);
uint8_t I2C_Probe(void);

#else

__attribute__((unused)) static int I2C_Master_Init(void) { return -1; }
__attribute__((unused)) static int I2C_Write(uint8_t *d, uint16_t l) { (void)d;(void)l; return -1; }
__attribute__((unused)) static int I2C_Read(uint8_t *d, uint16_t l) { (void)d;(void)l; return -1; }
__attribute__((unused)) static int I2C_WriteRead(uint8_t *t, uint16_t tl, uint8_t *r, uint16_t rl) { (void)t;(void)tl;(void)r;(void)rl; return -1; }
__attribute__((unused)) static uint8_t I2C_Probe(void) { return 0; }

#endif /* ENABLE_I2C_MASTER */

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H7_H */
