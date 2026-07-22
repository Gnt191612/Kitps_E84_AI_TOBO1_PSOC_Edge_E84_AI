/**
 * @file    j5_int.h
 * @brief   J5 中断线控制（H7→PSE84E 事件通知）
 *
 * H7 作为 Master，通过 GPIO 输出控制 4 路中断线：
 *   INT0: 雷达扫描完成 → 通知 84E 准备 NPU
 *   INT1: ESP32 跟踪事件转发 → 通知 84E
 *   INT2: 命令握手 → 在 I2C 写入后通知 84E 读取
 *   INT3: 多目标通知
 *
 * 电平：低电平触发（默认空闲高电平）
 * 引脚：GPIOB (PB8/PB9/PB12/PB13)
 *
 * 本头文件自包含引脚定义，不依赖 CubeMX main.h 中的 PIN 宏。
 */

#ifndef __J5_INT_H
#define __J5_INT_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- J5 中断引脚（GPIOB, 推挽输出, 低电平触发） ---- */
#define J5_INT_PORT         GPIOB

/* SERIAL_INT0: H7→84E，雷达扫描完成通知 */
#define J5_INT0_PIN         GPIO_PIN_8
/* SERIAL_INT1: H7→84E，ESP 跟踪事件转发 */
#define J5_INT1_PIN         GPIO_PIN_9
/* SERIAL_INT2: H7→84E，命令握手 */
#define J5_INT2_PIN         GPIO_PIN_12
/* SERIAL_INT3: H7→84E，多目标通知 */
#define J5_INT3_PIN         GPIO_PIN_13

/* 所有 INT 引脚掩码（用于批量初始化） */
#define J5_ALL_INT_MASK     (J5_INT0_PIN | J5_INT1_PIN | J5_INT2_PIN | J5_INT3_PIN)

/** 拉低 INTx 线（触发中断）后自动恢复高电平 */
/** @note 在 225MHz H7 上约 0.2µs/循环，5000 循环 ≈ 1ms，足够可靠检测 */
#define J5_PULSE_DELAY  5000U

static inline void J5_Pulse_INT0(void)
{
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT0_PIN, GPIO_PIN_RESET);
    for (volatile uint32_t i = 0; i < J5_PULSE_DELAY; i++);
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT0_PIN, GPIO_PIN_SET);
}

static inline void J5_Pulse_INT1(void)
{
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT1_PIN, GPIO_PIN_RESET);
    for (volatile uint32_t i = 0; i < J5_PULSE_DELAY; i++);
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT1_PIN, GPIO_PIN_SET);
}

static inline void J5_Pulse_INT2(void)
{
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT2_PIN, GPIO_PIN_RESET);
    for (volatile uint32_t i = 0; i < J5_PULSE_DELAY; i++);
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT2_PIN, GPIO_PIN_SET);
}

static inline void J5_Pulse_INT3(void)
{
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT3_PIN, GPIO_PIN_RESET);
    for (volatile uint32_t i = 0; i < J5_PULSE_DELAY; i++);
    HAL_GPIO_WritePin(J5_INT_PORT, J5_INT3_PIN, GPIO_PIN_SET);
}

/**
 * @brief 初始化所有 J5 INT 引脚为推挽输出，默认拉高（空闲高电平）
 * @note 在 System_Init 中调用
 */
static inline void J5_InitAll(void)
{
    GPIO_InitTypeDef gpio_int = {0};
    gpio_int.Pin   = J5_ALL_INT_MASK;
    gpio_int.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_int.Pull  = GPIO_NOPULL;
    gpio_int.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(J5_INT_PORT, &gpio_int);
    HAL_GPIO_WritePin(J5_INT_PORT, J5_ALL_INT_MASK, GPIO_PIN_SET);
}

#ifdef __cplusplus
}
#endif

#endif /* __J5_INT_H */
