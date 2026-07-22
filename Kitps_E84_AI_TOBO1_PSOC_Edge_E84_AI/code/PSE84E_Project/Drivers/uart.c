/**
 * @file    uart.c
 * @brief   UART 驱动实现（基于 Infineon PDL Cy_SCB_UART）
 *
 * PSoC Edge E84 用 SCB 模块实现 UART。
 * 参考 PDL API: Cy_SCB_UART_Init / Cy_SCB_UART_Transmit / Cy_SCB_UART_Receive
 */

#include "uart.h"
#include "gpio.h"
#include <string.h>

/* SCB UART 并发上下文（为每个 SCB 实例，SCB0 ~ SCB8） */
static cy_stc_scb_uart_context_t s_uartCtx[9];
static void (*s_rxCallback[9])(uint8_t byte) = { NULL };

int UART_Init(CySCB_Type *scb, uint32_t baud,
              GPIO_PRT_Type *port, uint32_t tx_pin, uint32_t rx_pin)
{
    if (!scb) return -1;

    /* 1. 配置 TX/RX 引脚为 SCB 复用功能 */
    GPIO_Init(port, tx_pin, GPIO_MODE_OUTPUT_PP);
    GPIO_Init(port, rx_pin, GPIO_MODE_INPUT);

    /* 2. 配置 UART */
    /* 计算 oversample: baud = SCB时钟 / oversample */
    /* 默认 oversample=8, 需确保调用前 SCB时钟 = baud * 8 */
    uint32_t ovs = 8UL;
    if (baud > 0) {
        /* 默认 SCB 时钟 1 MHz, 则 oversample = 1000000 / baud, 最小 8 */
        ovs = 1000000UL / baud;
        if (ovs < 8UL) ovs = 8UL;
        if (ovs > 16UL) ovs = 16UL;
    }

    cy_stc_scb_uart_config_t uartConfig = {
        .uartMode                 = CY_SCB_UART_STANDARD,
        .enableMutliProcessorMode = false,
        .smartCardRetryOnNack     = false,
        .irdaInvertRx             = false,
        .irdaEnableLowPowerReceiver = false,
        .oversample               = ovs,
        .enableMsbFirst           = false,
        .dataWidth                = 8UL,
        .parity                   = CY_SCB_UART_PARITY_NONE,
        .stopBits                 = CY_SCB_UART_STOP_BITS_1,
        .enableInputFilter        = false,
        .dropOnParityError        = false,
        .dropOnFrameError         = false,
        .enableCts                = false,
        .ctsPolarity              = CY_SCB_UART_ACTIVE_LOW,
        .rtsRxFifoLevel           = 0UL,
        .rtsPolarity              = CY_SCB_UART_ACTIVE_LOW,
        .breakWidth               = 0UL,
        .rxFifoTriggerLevel       = 0UL,
        .rxFifoIntEnableMask      = 0UL,
        .txFifoTriggerLevel       = 0UL,
        .txFifoIntEnableMask      = 0UL,
    };

    /* 计算 SCB 索引 */
    uint32_t scbIdx = 0;
    if      (scb == SCB0)  scbIdx = 0;
    else if (scb == SCB1)  scbIdx = 1;
    else if (scb == SCB2)  scbIdx = 2;
    else if (scb == SCB3)  scbIdx = 3;
    else if (scb == SCB4)  scbIdx = 4;
    else if (scb == SCB5)  scbIdx = 5;
    else if (scb == SCB6)  scbIdx = 6;
    else if (scb == SCB7)  scbIdx = 7;
    else if (scb == SCB8)  scbIdx = 8;
    else return -1;

    cy_en_scb_uart_status_t status;
    status = Cy_SCB_UART_Init(scb, &uartConfig, &s_uartCtx[scbIdx]);
    if (status != CY_SCB_UART_SUCCESS) return -1;

    Cy_SCB_UART_Enable(scb);

    /* 3. 使能 RX 中断 - 使用 Cy_SCB_SetRxInterruptMask */
    Cy_SCB_SetRxInterruptMask(scb, CY_SCB_UART_RX_NOT_EMPTY);

    return 0;
}

int UART_Send(CySCB_Type *scb, const uint8_t *data, uint16_t len)
{
    if (!scb || !data || len == 0) return 0;

    uint32_t scbIdx = 0;
    if      (scb == SCB0)  scbIdx = 0;
    else if (scb == SCB1)  scbIdx = 1;
    else if (scb == SCB2)  scbIdx = 2;
    else if (scb == SCB3)  scbIdx = 3;
    else if (scb == SCB4)  scbIdx = 4;
    else if (scb == SCB5)  scbIdx = 5;
    else if (scb == SCB6)  scbIdx = 6;
    else if (scb == SCB7)  scbIdx = 7;
    else if (scb == SCB8)  scbIdx = 8;
    else return 0;

    Cy_SCB_UART_Transmit(scb, (void *)data, len, &s_uartCtx[scbIdx]);

    /* 等待发送完成 */
    while (Cy_SCB_UART_IsTxComplete(scb) == false);

    return (int)len;
}

void UART_SetRxCallback(CySCB_Type *scb, void (*cb)(uint8_t byte))
{
    uint32_t scbIdx = 0;
    if      (scb == SCB0)  scbIdx = 0;
    else if (scb == SCB1)  scbIdx = 1;
    else if (scb == SCB2)  scbIdx = 2;
    else if (scb == SCB3)  scbIdx = 3;
    else if (scb == SCB4)  scbIdx = 4;
    else if (scb == SCB5)  scbIdx = 5;
    else if (scb == SCB6)  scbIdx = 6;
    else if (scb == SCB7)  scbIdx = 7;
    else if (scb == SCB8)  scbIdx = 8;
    else return;

    s_rxCallback[scbIdx] = cb;
}

void UART_IrqHandler(CySCB_Type *scb)
{
    uint32_t scbIdx = 0;
    if      (scb == SCB0)  scbIdx = 0;
    else if (scb == SCB1)  scbIdx = 1;
    else if (scb == SCB2)  scbIdx = 2;
    else if (scb == SCB3)  scbIdx = 3;
    else if (scb == SCB4)  scbIdx = 4;
    else if (scb == SCB5)  scbIdx = 5;
    else if (scb == SCB6)  scbIdx = 6;
    else if (scb == SCB7)  scbIdx = 7;
    else if (scb == SCB8)  scbIdx = 8;
    else return;

    uint32_t cause = Cy_SCB_GetInterruptCause(scb);

    if (cause & CY_SCB_RX_INTR) {
        while (Cy_SCB_UART_GetNumInRxFifo(scb) > 0) {
            uint8_t byte = (uint8_t)Cy_SCB_UART_Get(scb);
            if (s_rxCallback[scbIdx]) {
                s_rxCallback[scbIdx](byte);
            }
        }
    }

    /* 清除 RX 中断 */
    Cy_SCB_ClearRxInterrupt(scb, Cy_SCB_GetRxInterruptStatus(scb));
}
