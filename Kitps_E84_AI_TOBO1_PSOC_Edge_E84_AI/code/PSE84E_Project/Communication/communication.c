/**
 * @file    communication.c
 * @brief   84E 通信层 — 位操 UART on P21_2(TX)/P21_3(RX) + P21_4/P21_5 事件
 *
 * 通信架构（仅 P21 引脚可用）：
 *   - 数据通道：位操 UART @ 115200bps on P21_2(TX) / P21_3(RX)
 *     P21_2(原 INT0) → H7 PB7 (USART1_RX)
 *     P21_3(原 INT1) ← H7 PB6 (USART1_TX)
 *   - 事件通道：J5 GPIO 中断线
 *     P21_4(INT2) / P21_5(INT3) 保留
 *
 * 注：P21_2 和 P21_3 原来作为 INT0/INT1 的中断功能已被 UART 取代。
 */

#include "communication.h"
#include "bb_uart.h"           /* 位操 UART 驱动 */
#include "gpio.h"              /* GPIO 中断配置 */
#include <string.h>

/* ======================================================================== */
/* J5 GPIO 中断引脚（P21 端口 — P21_4/P21_5 保留用于事件通知）              */
/* ======================================================================== */
#define J5_PORT              GPIO_PRT21
#define J5_INT2_PIN          4U       /* P21_4 — 事件通知 */
#define J5_INT3_PIN          5U       /* P21_5 — 事件通知 */

/* ======================================================================== */
/* 回调函数指针                                                              */
/* ======================================================================== */
void (*g_int2_callback)(void) = NULL;
void (*g_int3_callback)(void) = NULL;

/* ======================================================================== */
/* UART 接收回调转发                                                        */
/* ======================================================================== */
static void (*g_rxByteCallback)(uint8_t byte) = NULL;

static void bb_rx_callback(uint8_t byte)
{
    if (g_rxByteCallback) {
        g_rxByteCallback(byte);
    }
}

/* ======================================================================== */
/* 公开接口实现                                                              */
/* ======================================================================== */

void Comm_Init(void)
{
    /* ---- 1. 初始化位操 UART (P21_2 TX, P21_3 RX, 115200) ---- */
    BB_UART_Init(bb_rx_callback);
    Comm_SetRxCallback(NULL);  /* 若协议层尚未注册，先清空 */

    /* ---- 2. 配置 J5 GPIO 中断引脚（P21_4/P21_5，下降沿触发） ---- */
    GPIO_SetISR(J5_PORT, J5_INT2_PIN, GPIO_IRQ_EDGE_FALLING, NULL);
    GPIO_SetISR(J5_PORT, J5_INT3_PIN, GPIO_IRQ_EDGE_FALLING, NULL);
}

void Comm_SendByte(uint8_t byte)
{
    BB_UART_SendByte(byte);
}

void Comm_SendData(uint8_t *data, uint16_t len)
{
    BB_UART_SendData(data, len);
}

void Comm_SetRxCallback(void (*callback)(uint8_t byte))
{
    g_rxByteCallback = callback;
}

void Comm_IRQHandler(void)
{
    /* 处理 J5 GPIO 中断（仅 P21_4, P21_5） */
    uint32_t intrStatus = Cy_GPIO_GetInterruptStatusMasked(J5_PORT, 0);

    if (intrStatus & (1UL << J5_INT2_PIN)) {
        Cy_GPIO_ClearInterrupt(J5_PORT, J5_INT2_PIN);
        if (g_int2_callback) g_int2_callback();
    }
    if (intrStatus & (1UL << J5_INT3_PIN)) {
        Cy_GPIO_ClearInterrupt(J5_PORT, J5_INT3_PIN);
        if (g_int3_callback) g_int3_callback();
    }
}

void Comm_InitInterrupts(void)
{
    /* BB UART RX 中断边缘在 GPIO_SetISR 中配置 */
    /* 实际 NVIC 使能在 GPIO_Interrupt_Init() 中完成 */
}

void Comm_SetInt2Callback(void (*callback)(void)) { g_int2_callback = callback; }
void Comm_SetInt3Callback(void (*callback)(void)) { g_int3_callback = callback; }

/* INT0/INT1 已被 UART TX/RX 占用，保留空桩保证链接不报错 */
void Comm_SetInt0Callback(void (*callback)(void)) { (void)callback; }
void Comm_SetInt1Callback(void (*callback)(void)) { (void)callback; }
