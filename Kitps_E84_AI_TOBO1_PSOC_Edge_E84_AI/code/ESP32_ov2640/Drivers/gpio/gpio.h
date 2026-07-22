/**
 * @file gpio.h
 * @brief ESP-IDF GPIO 驱动封装
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ──── GPIO 模式 ──── */
/* 为避免与 ESP-IDF hal/gpio_types.h 中的同名枚举冲突，使用本地别名 */
typedef enum {
    GPIO_MODE_INPUT_LOCAL  = GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT_LOCAL = GPIO_MODE_OUTPUT,
    GPIO_MODE_INPUT_OUTPUT_LOCAL = GPIO_MODE_INPUT_OUTPUT,
} GPIO_Mode_t;

/* ──── GPIO 中断回调 ──── */
typedef void (*GPIO_IsrHandler_t)(void *arg);

/* ──── 公共接口 ──── */

/**
 * @brief 初始化 GPIO
 * @param pin  GPIO 引脚号
 * @param mode 输入/输出模式
 * @param pull_up   是否上拉
 * @param pull_down 是否下拉
 * @return ESP_OK 成功
 */
int GPIO_Init(gpio_num_t pin, GPIO_Mode_t mode, int pull_up, int pull_down);

/**
 * @brief 写 GPIO 电平
 * @param pin  引脚号
 * @param val  0=低电平, 1=高电平
 */
void GPIO_Write(gpio_num_t pin, uint8_t val);

/**
 * @brief 读 GPIO 电平
 * @param pin  引脚号
 * @return 0=低, 1=高
 */
int GPIO_Read(gpio_num_t pin);

/**
 * @brief 注册 GPIO 中断
 * @param pin  引脚号
 * @param cb   中断回调
 * @param arg  回调参数
 * @param edge 触发边沿 (GPIO_INTR_POSEDGE / GPIO_INTR_NEGEDGE / GPIO_INTR_ANYEDGE)
 * @return ESP_OK 成功
 */
int GPIO_IsrRegister(gpio_num_t pin, GPIO_IsrHandler_t cb, void *arg, gpio_int_type_t edge);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */
