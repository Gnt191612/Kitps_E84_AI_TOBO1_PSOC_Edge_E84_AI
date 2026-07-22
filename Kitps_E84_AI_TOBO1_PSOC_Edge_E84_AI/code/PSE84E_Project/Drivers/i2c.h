/**
 * @file    i2c.h
 * @brief   I2C 驱动封装（基于 Infineon PDL Cy_SCB_I2C — Slave 模式）
 *
 * PSE84E 作为 I2C Slave，与 STM32H7 (Master) 通信。
 * 使用 J5 连接器（P21 端口）的 I3C_SDA/I3C_SCL 引脚。
 *
 * 接口：
 *   I2C_Slave_Init()    - 初始化 I2C Slave
 *   I2C_Slave_Deinit()  - 反初始化
 *   I2C_Slave_Poll()    - 轮询处理接收数据（若未用中断）
 *   I2C_GetWriteBuf()   - 获取 Master 写入的数据指针和长度
 *   I2C_GetReadBuf()    - 获取 Master 待读取的数据缓冲指针
 *   I2C_CommitReadBuf() - 将数据提交到读缓冲，标记 Master 可读
 *   I2C_IsWritePending()- 查询是否有新的写数据待处理
 */

#ifndef __I2C_H
#define __I2C_H

#include <stdint.h>
#include "cy_pdl.h"
#include "main.h"  /* NVIC 宏定义 */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/* I2C 配置参数（可根据实际需要修改）                                          */
/* ======================================================================== */

/* PSE84E 作为 I2C Slave 的 7 位地址 */
#define I2C_SLAVE_ADDR          0x42U

/* I2C 读写缓冲区大小（字节），需能容纳最大帧 */
#define I2C_BUF_SIZE            128U

/* 使用的 SCB 实例 — PDL 文档以 SCB3 作为 I2C 示例, */
/* 同时 P21 的 I3C/I2C 功能在 TOBO1 板上路由到 SCB3       */
#ifndef I2C_SCB
#define I2C_SCB                 SCB3
#endif

/* HSIOM 选择值（将 P21_0/P21_1 路由到 SCB3 I2C 功能）    */
/* 在 XMC8400E 数据手册「HSIOM 路由表」中查找：            */
/*   P21_0 → SCB3_I2C_SDA  的 HSIOM_SEL                   */
/*   P21_1 → SCB3_I2C_SCL  的 HSIOM_SEL                   */
/* CAT1C 典型规律：SCB0=2, SCB1=3, SCB2=4, SCB3=5, ...    */
/* 以下用 5U 作为 SCB3 I2C 的 HSIOM 值（标准 CAT1C 路由） */
#ifndef I2C_SDA_HSIOM_SEL
#define I2C_SDA_HSIOM_SEL       5U
#endif
#ifndef I2C_SCL_HSIOM_SEL
#define I2C_SCL_HSIOM_SEL       5U
#endif

/* IRQ 号（对应 scb_3_interrupt_IRQn = 105） */
#define I2C_SCB_IRQ             scb_3_interrupt_IRQn

/* ======================================================================== */
/* 公开接口                                                                  */
/* ======================================================================== */

/**
 * @brief 初始化 I2C Slave 模式
 * @param sda_port  I2C SDA 端口指针
 * @param sda_pin   I2C SDA 引脚编号
 * @param scl_port  I2C SCL 端口指针
 * @param scl_pin   I2C SCL 引脚编号
 * @param slave_addr 7 位从机地址
 * @return 0=成功, -1=失败
 */
int I2C_Slave_Init(GPIO_PRT_Type *sda_port, uint32_t sda_pin,
                   GPIO_PRT_Type *scl_port, uint32_t scl_pin,
                   uint8_t slave_addr);

/**
 * @brief 反初始化 I2C Slave
 */
void I2C_Slave_Deinit(void);

/**
 * @brief 轮询 I2C 事件（在无中断或需要额外轮询时调用）
 * 检查 Master 是否写入了新数据，若有则触发回调
 */
void I2C_Slave_Poll(void);

/**
 * @brief 获取 Master 写入的缓冲区和长度（在回调或轮询中调用）
 * @param out_len 传出：写入的字节数
 * @return 写入数据缓冲区指针
 */
uint8_t *I2C_GetWriteBuf(uint16_t *out_len);

/**
 * @brief 获取 Master 读取缓冲区指针（用于填充回复数据）
 * @return 读缓冲区指针
 */
uint8_t *I2C_GetReadBuf(void);

/**
 * @brief 提交读缓冲区数据，标记 Master 可读取
 * @param len 已写入读缓冲区的字节数
 */
void I2C_CommitReadBuf(uint16_t len);

/**
 * @brief 检查是否有新的写数据待处理
 * @return 1=有, 0=无
 */
uint8_t I2C_IsWritePending(void);

/**
 * @brief 设置写数据到达回调
 * @param cb 回调函数（接收缓冲区指针和长度）
 */
void I2C_SetRxCallback(void (*cb)(uint8_t *data, uint16_t len));

/**
 * @brief I2C 中断处理入口（在 SCB IRQHandler 中调用）
 */
void I2C_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H */
