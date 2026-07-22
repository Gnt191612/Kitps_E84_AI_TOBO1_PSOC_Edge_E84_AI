/**
 * @file bb_uart.c
 * @brief 位操 UART 实现 (Bit-banged UART on P21_2/P21_3 @ 115200)
 *
 * 实现原理：
 *   - 发送：GPIO 直接以 115200 baud 的时序输出串行帧 (start+8data+stop)
 *   - 接收：P21_3 下降沿中断检测起始位 → 采样剩余位 → 推入环形缓冲区
 *   - 精度：DWT CYCCNT 提供微秒级延迟 (~160 cycles/μs @ 160MHz)
 *
 * 发送帧格式：
 *   [start=0][bit0][bit1]...[bit7][stop=1]
 *   LSB first, 1 start bit, 8 data bits, 1 stop bit
 *
 * 接收帧格式：
 *   下降沿中断 → 等待半位 → 采样8位 → 等待停止位
 */

#include "bb_uart.h"
#include "gpio.h"
#include "logger.h"
#include "XMC8400E.h"
#include <string.h>

/* ======================================================================== */
/* DWT CYCCNT 宏（来自 main.h）                                              */
/* ======================================================================== */
#ifndef DWT_CYCCNT
#define DWT_CYCCNT          (*((volatile uint32_t *)0xE0001004UL))
#endif

/* ======================================================================== */
/* 内部状态                                                                  */
/* ======================================================================== */

/** 接收字节回调（注册到 communication.c 的协议层） */
static void (*s_rx_callback)(uint8_t byte) = NULL;

/** 接收缓冲区（环形队列） */
static volatile uint8_t  s_rx_buf[BB_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;
static volatile uint8_t  s_rx_busy = 0;     /* 当前正在接收一字节的忙标志 */

/* ======================================================================== */
/* 精确微秒延迟 — 基于 DWT CYCCNT @ 160MHz                                 */
/* ======================================================================== */
static inline void delay_us(uint32_t us)
{
    uint32_t start = DWT_CYCCNT;
    uint32_t ticks = us * 160UL;  /* 160 MHz = 160 cycles/μs */
    while ((DWT_CYCCNT - start) < ticks) { }
}

/* ======================================================================== */
/* 发送一个字节（阻塞）                                                       */
/* ======================================================================== */
void BB_UART_SendByte(uint8_t byte)
{
    /* 关中断（防止接收 ISR 干扰发送时序） */
    __disable_irq();

    /* Start bit: 拉低 */
    GPIO_WritePin(BB_TX_PORT, BB_TX_PIN, 0);
    delay_us(BB_BIT_US);

    /* 8 数据位, LSB first */
    for (int i = 0; i < 8; i++) {
        GPIO_WritePin(BB_TX_PORT, BB_TX_PIN, (byte >> i) & 1);
        delay_us(BB_BIT_US);
    }

    /* Stop bit: 拉高 */
    GPIO_WritePin(BB_TX_PORT, BB_TX_PIN, 1);
    delay_us(BB_BIT_US);

    /* 恢复中断 */
    __enable_irq();
}

/* ======================================================================== */
/* 发送多个字节                                                               */
/* ======================================================================== */
void BB_UART_SendData(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        BB_UART_SendByte(data[i]);
    }
}

/* ======================================================================== */
/* 接收中断 ISR — 由 P21_3 下降沿触发                                       */
/*                                                                           */
/* 当一个字节到齐后，回调 s_rx_callback。                                    */
/* ======================================================================== */
void BB_UART_RxISR(void)
{
    if (s_rx_busy) {
        /* 上一个字节还没收完，放弃这个 */
        return;
    }
    s_rx_busy = 1;

    /* ---- 收到起始位（下降沿），等待半位到采样中点 ---- */
    delay_us(BB_BIT_US / 2);

    /* ---- 采样 8 位数据 ---- */
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        delay_us(BB_BIT_US);                     /* 等待下一个位 */
        if (GPIO_ReadPin(BB_RX_PORT, BB_RX_PIN)) {
            byte |= (1 << i);                    /* LSB first */
        }
    }

    /* ---- 等待停止位 ---- */
    delay_us(BB_BIT_US);

    s_rx_busy = 0;

    /* ---- 推入环形缓冲区 ---- */
    uint16_t next = (s_rx_head + 1) % BB_RX_BUF_SIZE;
    if (next != s_rx_tail) {
        s_rx_buf[s_rx_head] = byte;
        s_rx_head = next;
    }

    /* ---- 回调 ---- */
    if (s_rx_callback) {
        s_rx_callback(byte);
    }
}

/* ======================================================================== */
/* 获取接收回调                                                              */
/* ======================================================================== */
void (*BB_UART_GetRxCallback(void))(uint8_t)
{
    return s_rx_callback;
}

/* ======================================================================== */
/* 初始化                                                                    */
/* ======================================================================== */
void BB_UART_Init(void (*rx_callback)(uint8_t byte))
{
    s_rx_callback = rx_callback;
    s_rx_head = 0;
    s_rx_tail = 0;
    s_rx_busy = 0;

    /* TX (P21_2): 推挽输出, 默认为高电平（UART 空闲态） */
    GPIO_WritePin(BB_TX_PORT, BB_TX_PIN, 1);
    GPIO_Init(BB_TX_PORT, BB_TX_PIN, GPIO_MODE_OUTPUT_PP);

    /* RX (P21_3): 输入（上升/下降沿已在 GPIO_Init 后通过 GPIO_SetISR 配置） */
    GPIO_Init(BB_RX_PORT, BB_RX_PIN, GPIO_MODE_INPUT);

    Logger_Print(LOG_INFO, "BB UART: P21_2(TX) P21_3(RX) @ 115200");
}
