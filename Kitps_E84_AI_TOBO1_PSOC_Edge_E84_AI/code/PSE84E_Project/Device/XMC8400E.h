/**
 * @file    XMC8400E.h
 * @brief   PSoC Edge E84 (CYT3BB5CEE) 设备头文件
 *
 * PSoC Edge E84 = Infineon CAT1C 系列
 * 使用 Infineon PDL (mtb-pdl-cat1) 驱动库
 *
 * PDL 包含路径（ModusToolbox 环境）：
 *   libs/mtb-pdl-cat1/drivers/include/
 *   libs/mtb-pdl-cat1/devices/COMPONENT_CAT1C/include/
 *
 * 不使用裸寄存器方式（已弃用 STM32 风格的模拟寄存器）
 */

#ifndef XMC8400E_H
#define XMC8400E_H

/* =====================================================================
 * 通过 cy_pdl.h 引入 PDL 设备头文件 + 全部外设驱动API
 *
 * cy_pdl.h 会：
 *   1. 根据编译宏自动包含正确的设备头文件 (cyt3bb5cee.h)
 *   2. 定义外设基地址指针 (SCB0, GPIO_PRT0, TCPWM0 等)
 *   3. 包含所有外设驱动 API 原型
 * ===================================================================== */
#include "cy_pdl.h"

/* =====================================================================
 * 遗留宏兼容（原代码引用的名称映射到 PDL 名称）
 * ===================================================================== */

/* NVIC / SCB 系统控制（CMSIS 标准） */
#define SCB_CPACR              (*(volatile uint32_t *)(0xE000ED88UL))
#define SCB_CPACR_CP10         (3UL << 20)
#define SCB_CPACR_CP11         (3UL << 22)

/* NPU 自定义寄存器（非 PDL 范围，需自行定义） */
/* 注意：实际使用 NNLite 0x40080000，见 npu.h */
#ifndef NPU_BASE
#define NPU_BASE               0x40080000UL    /* NNLite 基地址（参考 npu.h） */
#endif

#define __IO                    volatile

/* 中断优先级寄存器（CMSIS 标准） */
#ifndef NVIC_SetPriority
#define NVIC_SetPriority(irq, prio)  NVIC_SetPriority(irq, prio)
#endif

/* =====================================================================
 * 提示：需要使用的 PDL API
 *
 * #include "cy_pdl.h" 后可直接使用以下 API：
 *
 * GPIO (IOSS):
 *   cy_stc_gpio_pin_config_t config = {
 *       .outVal = 0,
 *       .driveMode = CY_GPIO_DM_STRONG_IN_OFF,
 *       .hyt = 0,
 *       .inputBuf = 0,
 *       .vTrip = 0,
 *       .slewRate = 0,
 *   };
 *   Cy_GPIO_PinInit(GPIO_PRTx, pin_num, &config);
 *   Cy_GPIO_Write(GPIO_PRTx, pin_num, value);
 *   uint8_t val = Cy_GPIO_Read(GPIO_PRTx, pin_num);
 *
 * UART (SCB):
 *   cy_stc_scb_uart_context_t uartContext;
 *   Cy_SCB_UART_Init(SCBx, &uartConfig, &uartContext);
 *   Cy_SCB_UART_Enable(SCBx);
 *   Cy_SCB_UART_Transmit(SCBx, buf, size, &uartContext);
 *   Cy_SCB_UART_Receive(SCBx, buf, size, &uartContext);
 *
 * TCPWM:
 *   cy_stc_tcpwm_pwm_config_t pwmConfig;
 *   Cy_TCPWM_PWM_Init(TCPWMx, groupNum, &pwmConfig);
 *   Cy_TCPWM_PWM_Enable(TCPWMx, groupNum);
 *   Cy_TCPWM_TriggerStart(TCPWMx, 1UL << groupNum);
 *   uint32_t cnt = Cy_TCPWM_GetCounter(TCPWMx, groupNum);
 *
 * 系统时钟 (SRSS):
 *   uint32_t periFreq = Cy_SysClk_ClkPeriGetFreq();
 *   uint32_t slowFreq = Cy_SysClk_ClkSlowGetFreq();
 *   Cy_SysLib_DelayUs(100);
 *
 * ===================================================================== */

#endif /* XMC8400E_H */
