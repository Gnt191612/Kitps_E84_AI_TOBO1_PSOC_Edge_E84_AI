/**
 * @file camera_io.h
 * @brief OV2640 SCCB (I2C-like) 寄存器读写
 *
 * 使用 ESP-IDF I2C 驱动模拟 SCCB 协议对 OV2640 寄存器读写。
 * SCCB 类似 I2C, 8-bit 地址 + 8-bit 数据。
 */

#ifndef CAMERA_IO_H
#define CAMERA_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── OV2640 SCCB 从机地址 ──── */
#define OV2640_SCCB_ADDR        0x60   /* 7-bit 地址 (0x30 << 1) */
#define OV2640_SCCB_ADDR_8BIT   0x60   /* 写: 0x60, 读: 0x61 */

/* ──── 公共接口 ──── */

/**
 * @brief 初始化 SCCB (I2C) 总线
 * @param sda_pin SDA GPIO
 * @param scl_pin SCL GPIO
 * @param clk_hz  I2C 时钟频率 (通常 100000 ~ 400000)
 * @return 0=成功, -1=失败
 */
int Cam_Init(int sda_pin, int scl_pin, uint32_t clk_hz);

/**
 * @brief 写 OV2640 寄存器
 * @param slave_id 从机地址
 * @param reg_addr 寄存器地址 (8-bit)
 * @param value    写入值
 * @return 0=成功, -1=失败
 */
int Cam_WriteReg(uint8_t slave_id, uint8_t reg_addr, uint8_t value);

/**
 * @brief 读 OV2640 寄存器
 * @param slave_id 从机地址
 * @param reg_addr 寄存器地址 (8-bit)
 * @param value    输出读取值
 * @return 0=成功, -1=失败
 */
int Cam_ReadReg(uint8_t slave_id, uint8_t reg_addr, uint8_t *value);

/**
 * @brief 写多字节 (SCCB 多字节写, 地址自动递增)
 * @param slave_id  从机地址
 * @param reg_addr  起始寄存器地址
 * @param data      数据指针
 * @param len       字节数
 * @return 0=成功, -1=失败
 */
int Cam_WriteMulti(uint8_t slave_id, uint8_t reg_addr, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_IO_H */
