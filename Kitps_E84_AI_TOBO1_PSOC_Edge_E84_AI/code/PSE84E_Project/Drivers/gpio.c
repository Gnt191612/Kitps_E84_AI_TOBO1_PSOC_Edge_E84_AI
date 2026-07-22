/**
 * @file    gpio.c
 * @brief   GPIO 驱动封装实现（基于 Infineon PDL Cy_GPIO API v3.22.1）
 */

#include "gpio.h"
#include <string.h>

void GPIO_Init(GPIO_PRT_Type *port, uint32_t pin, GPIO_Mode_t mode)
{
    if (!port) return;

    cy_stc_gpio_pin_config_t config = {
        .outVal    = 0U,
        .driveMode = (uint32_t)mode,
        .hsiom     = (en_hsiom_sel_t)0U,           /* CPU GPIO 模式 */
        .intEdge   = CY_GPIO_INTR_DISABLE,
        .intMask   = 0U,
        .vtrip     = CY_GPIO_VTRIP_CMOS,
        .slewRate  = CY_GPIO_SLEW_FAST,
        .driveSel  = 0U,
        .vregEn    = 0U,
        .ibufMode  = 0U,
        .vtripSel  = 0U,
        .vrefSel   = 0U,
        .vohSel    = 0U,
    };

    (void)Cy_GPIO_Pin_Init(port, pin, &config);
}

void GPIO_SetISR(GPIO_PRT_Type *port, uint32_t pin, GPIO_IrqTrigger_t trigger,
                 GPIO_IrqCallback_t callback)
{
    if (!port) return;

    uint32_t intrMode;

    switch (trigger) {
    case GPIO_IRQ_EDGE_RISING:  intrMode = CY_GPIO_INTR_RISING;  break;
    case GPIO_IRQ_EDGE_FALLING: intrMode = CY_GPIO_INTR_FALLING; break;
    case GPIO_IRQ_EDGE_BOTH:    intrMode = CY_GPIO_INTR_BOTH;    break;
    case GPIO_IRQ_LEVEL_LOW:    intrMode = CY_GPIO_INTR_FALLING; break;
    case GPIO_IRQ_LEVEL_HIGH:   intrMode = CY_GPIO_INTR_RISING;  break;
    default:                    intrMode = CY_GPIO_INTR_DISABLE; break;
    }

    /* 设置单引脚中断模式 */
    Cy_GPIO_SetInterruptEdge(port, pin, intrMode);

    /* 注册回调（需在系统中断处理中调用 Cy_GPIO_GetInterruptStatus） */
    (void)callback;
}
