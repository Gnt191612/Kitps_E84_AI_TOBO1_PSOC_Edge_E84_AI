/**
 * @file    gpio.h
 * @brief   GPIO 驱动封装（基于 Infineon PDL Cy_GPIO API）
 *
 * 底层依赖：cy_pdl.h → Cy_GPIO_* 函数
 *
 * 公开接口：
 *   GPIO_Init()        - 初始化引脚模式
 *   GPIO_WritePin()    - 写引脚电平
 *   GPIO_ReadPin()     - 读引脚电平
 *   GPIO_SetISR()      - 设置引脚中断回调
 */

#ifndef __GPIO_H
#define __GPIO_H

#include <stdint.h>
#include "cy_pdl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 引脚模式枚举（映射到 PDL Cy_GPIO_DriveMode_t） */
typedef enum {
    GPIO_MODE_INPUT           = CY_GPIO_DM_HIGHZ,           /* 高阻输入              */
    GPIO_MODE_INPUT_PULLUP    = CY_GPIO_DM_PULLUP,          /* 上拉输入              */
    GPIO_MODE_INPUT_PULLDOWN  = CY_GPIO_DM_PULLDOWN,         /* 下拉输入              */
    GPIO_MODE_OUTPUT_PP       = CY_GPIO_DM_STRONG_IN_OFF,   /* 推挽输出              */
    GPIO_MODE_OUTPUT_OD       = CY_GPIO_DM_OD_DRIVESLOW,    /* 开漏输出（低态驱动）  */
    GPIO_MODE_ANALOG          = CY_GPIO_DM_ANALOG            /* 模拟模式              */
} GPIO_Mode_t;

/* 引脚中断触发方式 */
typedef enum {
    GPIO_IRQ_EDGE_RISING  = 0,
    GPIO_IRQ_EDGE_FALLING = 1,
    GPIO_IRQ_EDGE_BOTH    = 2,
    GPIO_IRQ_LEVEL_LOW    = 3,
    GPIO_IRQ_LEVEL_HIGH   = 4
} GPIO_IrqTrigger_t;

/* 引脚中断回调类型 */
typedef void (*GPIO_IrqCallback_t)(void);

/**
 * @brief 初始化一个 GPIO 引脚
 * @param port   GPIO 端口指针（如 GPIO_PRT0）
 * @param pin    引脚编号 (0~31)
 * @param mode   引脚模式（GPIO_Mode_t）
 */
void GPIO_Init(GPIO_PRT_Type *port, uint32_t pin, GPIO_Mode_t mode);

/**
 * @brief 写引脚电平
 * @param port   GPIO 端口指针
 * @param pin    引脚编号
 * @param val    0=低电平, 1=高电平
 */
static inline void GPIO_WritePin(GPIO_PRT_Type *port, uint32_t pin, uint32_t val)
{
    Cy_GPIO_Write(port, pin, val);
}

/**
 * @brief 读引脚电平
 * @param port GPIO 端口指针
 * @param pin  引脚编号
 * @return 0=低电平, 1=高电平
 */
static inline uint8_t GPIO_ReadPin(GPIO_PRT_Type *port, uint32_t pin)
{
    return Cy_GPIO_Read(port, pin);
}

/**
 * @brief 翻转引脚电平
 * @param port GPIO 端口指针
 * @param pin  引脚编号
 */
static inline void GPIO_TogglePin(GPIO_PRT_Type *port, uint32_t pin)
{
    Cy_GPIO_Inv(port, pin);
}

/**
 * @brief 设置引脚中断
 * @param port    GPIO 端口指针
 * @param pin     引脚编号
 * @param trigger 触发方式
 * @param callback 中断回调函数（在 ISR 中调用）
 */
void GPIO_SetISR(GPIO_PRT_Type *port, uint32_t pin, GPIO_IrqTrigger_t trigger,
                 GPIO_IrqCallback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H */
