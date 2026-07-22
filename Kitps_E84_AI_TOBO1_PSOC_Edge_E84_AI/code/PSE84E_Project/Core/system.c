/**
 * @file    system.c
 * @brief   系统初始化实�? *
 * 完整初始化顺序：
 *   SystemInit (启动文件) �?FPU 使能 �?IMO �?FLL �?PLL �?160 MHz
 *   System_Init (main.c)  �?HAL_Init �?时钟 �?GPIO �?TIM
 *                           �?Logger �?Perf �?ErrorRate �?Comm (SCB0 UART) �?NPU
 *
 * 通信方案（与接线指南一致）�? *   数据通道：SCB0 UART (P0.0/P0.1, 115200bps) �?H7 USART1
 *   事件通道：J5 四路 GPIO 中断�?(P21_2~P21_5, 低电平触�?
 *   J5 SDA/SCL (P21_0/P21_1) 物理上未连接，不使用 I2C
 *
 * 外设引脚分配�? *   J5 I2C (I3C_SDA/SCL)  : GPIO_PRT21, P21_0=SDA, P21_1=SCL
 *   J5 INT0 (雷达扫描完成) : GPIO_PRT21, P21_2
 *   J5 INT1 (ESP跟踪事件)  : GPIO_PRT21, P21_3
 *   J5 INT2 (H7命令握手)   : GPIO_PRT21, P21_4
 *   J5 INT3 (多目标检�?   : GPIO_PRT21, P21_5
 *   LED                   : GPIO_PRT1, P1.7
 *
 * @note HC-SR04 和平台舵机由 STM32H7 直连控制，PSE84E 不直连�? */

#include "system.h"
#include "main.h"
#include "logger.h"
#include "performance.h"
#include "error_rate.h"
#include "communication.h"
#include "npu.h"
#include "gpio.h"
#include "timer.h"
#include "XMC8400E.h"
#include "ov7675.h"
#include "bb_uart.h"
#include "infer.h"
#include "radar.h"

/* ======================================================================== */
/* J5 (P21 端口) 引脚宏定�?                                                  */
/* ======================================================================== */

/* I2C 通信总线（通过 J5 QWIIC 连接器） */
#define J5_SDA_PORT        GPIO_PRT21
#define J5_SDA_PIN         0u
#define J5_SCL_PORT        GPIO_PRT21
#define J5_SCL_PIN         1u

/* GPIO 中断�?*/
#define J5_INT0_PORT       GPIO_PRT21
#define J5_INT0_PIN        2u
#define J5_INT1_PORT       GPIO_PRT21
#define J5_INT1_PIN        3u
#define J5_INT2_PORT       GPIO_PRT21
#define J5_INT2_PIN        4u
#define J5_INT3_PORT       GPIO_PRT21
#define J5_INT3_PIN        5u

/* 便捷读宏（与 J5 信号清单一致） */
#define RADAR_SCAN_INT      Cy_GPIO_Read(J5_INT0_PORT, J5_INT0_PIN)
#define ESP_TRACK_INT       Cy_GPIO_Read(J5_INT1_PORT, J5_INT1_PIN)
#define STM_CMD_HS_INT      Cy_GPIO_Read(J5_INT2_PORT, J5_INT2_PIN)
#define MULTI_TARGET_INT    Cy_GPIO_Read(J5_INT3_PORT, J5_INT3_PIN)

/* ======================================================================== */
/* SCB0 UART �?当前使用的通信通道 (P0.0/P0.1, 115200bps, �?H7 USART1 交叉连接) */
#define BB_UART_BAUD       115200

/* 状�?LED */
#define LED_PORT           GPIO_PRT1
#define LED_PIN            7

/* 系统滴答定时器（SysTick 周期�?*/
#define SYSTICK_PERIOD_MS  1U

/* ======================================================================== */
/* 全局变量                                                                  */
/* ======================================================================== */

/* �?main.h 中的 extern 引用 */
cy_stc_scb_uart_context_t g_scb_uart_context;

/* SysTick 累计毫秒�?*/
volatile uint32_t g_sysTickMs = 0;

/* ======================================================================== */
/* Systick 中断处理�?ms 中断�?                                              */
/* ======================================================================== */
void SysTick_Handler(void)
{
    g_sysTickMs++;
}

/* ======================================================================== */
/* HAL_GetTick �?返回系统启动以来的毫秒数                                     */
/* ======================================================================== */
uint32_t HAL_GetTick(void)
{
    return g_sysTickMs;
}

/* ======================================================================== */
/* GetMicros �?返回系统启动以来的微秒数（基�?DWT CYCCNT�?                    */
/* ======================================================================== */
uint32_t GetMicros(void)
{
    static uint32_t last_cnt = 0;
    static uint32_t wrap_ms  = 0;
    uint32_t cnt = DWT_CYCCNT;
    uint32_t core_hz = 160000000UL;  /* 与实际系统时钟一�?*/

    /* 检�?32 位溢出（�?26.8 �?@ 160 MHz�?*/
    if (cnt < last_cnt) {
        wrap_ms += 26843UL;  /* 2^32 / 160M * 1000 */
    }
    last_cnt = cnt;

    return (cnt / (core_hz / 1000000UL)) + wrap_ms;
}

/* ======================================================================== */
/* HAL_Init �?基础硬件初始�?                                                */
/* ======================================================================== */
void HAL_Init(void)
{
    /* 1. 使能 DWT 周期计数器（�?GetMicros 使用�?*/
    *((volatile uint32_t *)0xE000EDFC) |= (1UL << 24);  /* TRCENA */
    *((volatile uint32_t *)0xE0001000) |= (1UL << 0);   /* CYCCNTENA */
    *((volatile uint32_t *)0xE0001004) = 0;              /* CYCCNT 清零 */

    /* 2. 配置 SysTick �?1ms 中断（使�?IMO/FLL 时钟源） */
    uint32_t sysclk = 8000000UL;  /* 初始 IMO 8 MHz */
    SysTick_Config(sysclk / 1000UL);  /* 1ms 中断 */

    /* 3. 使能全局中断 */
    __enable_irq();
}

/* ======================================================================== */
/* SystemClock_Config �?配置 PLL 输出 160 MHz 系统时钟                        */
/* ======================================================================== */
void SystemClock_Config(void)
{
    /* 先使�?IMO（确保参考时钟就绪） */
    Cy_SysClk_ImoEnable();

    /* 配置 PLL200M �?160 MHz */
    cy_stc_pll_config_t pllConfig = {
        .inputFreq  = 8000000UL,      /* IMO 8 MHz */
        .outputFreq = 160000000UL,    /* 目标 160 MHz */
        .lfMode     = false,
        .outputMode = CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT,
    };

    cy_en_sysclk_status_t clkStatus;
    clkStatus = Cy_SysClk_Pll200MConfigure(0UL, &pllConfig);
    if (clkStatus == CY_SYSCLK_SUCCESS) {
        Cy_SysClk_Pll200MEnable(0UL, 1000);
    }

    /* 使能 FLL �?100 MHz（外设时钟源�?*/
    Cy_SysClk_FllConfigure(8000000UL, 100000000UL, CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT);
    Cy_SysClk_FllEnable(1000UL);

    /* 外设时钟分频 */
    Cy_SysClk_ClkPeriSetDivider(0);

    /* 重新配置 SysTick �?PLL 已使能，内核时钟 = 160MHz */
    SysTick_Config(160000000UL / 1000UL);

    /* 更新系统时钟 = 160 MHz */
    SystemCoreClock = 160000000UL;
}

/* ======================================================================== */
/* MX_GPIO_Init �?GPIO 外设初始�?                                           */
/* ======================================================================== */
void MX_GPIO_Init(void)
{
    /* 状�?LED */
    GPIO_Init(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT_PP);
    GPIO_WritePin(LED_PORT, LED_PIN, 0);  /* 默认�?*/

    /* ---- J5 中断引脚：默认初始化为输�?---- */
    GPIO_Init(J5_INT0_PORT, J5_INT0_PIN, GPIO_MODE_INPUT);
    GPIO_Init(J5_INT1_PORT, J5_INT1_PIN, GPIO_MODE_INPUT);
    GPIO_Init(J5_INT2_PORT, J5_INT2_PIN, GPIO_MODE_INPUT);
    GPIO_Init(J5_INT3_PORT, J5_INT3_PIN, GPIO_MODE_INPUT);

    /* J5 SDA/SCL (P21_0/P21_1) 未连接，仅初始化四路 INT 引脚 */
}

/* ======================================================================== */
/* MX_UART_Init �?SCB0 UART 初始�?                                         */
/* 实际初始化在 Comm_Init() 中由 UART_Init() 完成�?                         */
/* ======================================================================== */
void MX_UART_Init(void)
{
    /* UART 初始化已移至 Comm_Init() */
}

/* ======================================================================== */
/* MX_TIM_Init �?定时器外设初始化                                              */
/* ======================================================================== */
void MX_TIM_Init(void)
{
    /* 定时器初始化（用于本地计时，非通信使用�?*/
}

/* ======================================================================== */
/* ======================================================================== */
/* SCB3 I2C 中断处理入口 �?J5 SDA/SCL 未连接，保留为空                        */
/* ======================================================================== */
void scb_3_interrupt_handler(void)
{
    /* J5 SDA/SCL (P21_0/P21_1) 未连接，�?ISR 不会触发 */
}

/* ======================================================================== */
/* GPIO 综合中断处理（IRQ 21）�?P21 �?4 �?SERIAL_INT 由统一入口处理        */
/* 中断 ISR 声明在下方 */
/* ======================================================================== */

/* 声明 communication.c 中的回调函数指针 */
extern void (*g_int0_callback)(void);
extern void (*g_int1_callback)(void);
extern void (*g_int2_callback)(void);
extern void (*g_int3_callback)(void);

/* GPIO 综合中断处理 */
void ioss_interrupt_gpio_dpslp_handler(void)
{
    uint32_t intrStatus = Cy_GPIO_GetInterruptStatusMasked(GPIO_PRT21, 0);

    if (intrStatus & (1UL << 2)) {
        Cy_GPIO_ClearInterrupt(GPIO_PRT21, 2);
    }
    if (intrStatus & (1UL << 3)) {
        Cy_GPIO_ClearInterrupt(GPIO_PRT21, 3);
        BB_UART_RxISR();
    }
    if (intrStatus & (1UL << 4)) {
        Cy_GPIO_ClearInterrupt(GPIO_PRT21, 4);
        if (g_int2_callback) g_int2_callback();
    }
    if (intrStatus & (1UL << 5)) {
        Cy_GPIO_ClearInterrupt(GPIO_PRT21, 5);
        if (g_int3_callback) g_int3_callback();
    }
}

/* 函数指针已在上面 ISR 前声�?*/

void GPIO_Interrupt_Init(void)
{
    /* 使能 GPIO 综合中断（IRQ 21 = ioss_interrupt_gpio_dpslp_IRQn�?*/
    NVIC_SetPriority(ioss_interrupt_gpio_dpslp_IRQn, 3U);
    NVIC_EnableIRQ(ioss_interrupt_gpio_dpslp_IRQn);
}

/* ======================================================================== */
/* System_Init �?系统整体初始�?                                              */
/* ======================================================================== */
void System_Init(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_UART_Init();    /* 空，保留兼容 */
    MX_TIM_Init();

    Logger_Init();
    Logger_SetLevel(LOG_INFO);
    Logger_Print(LOG_INFO, "PSE84E System Init Start (UART + J5 INT)...");

    Perf_Init();
    ErrorRate_Init();

    /* 注意顺序：Comm_Init 必须�?Protocol_Init 之前 */
    Comm_Init();        /* 初始�?SCB0 UART + J5 GPIO 中断引脚 */

    /* 注册 J5 中断回调并启用中�?*/
    extern void Scheduler_Int0Callback(void);
    extern void Scheduler_Int1Callback(void);
    extern void Scheduler_Int2Callback(void);
    extern void Scheduler_Int3Callback(void);

    Comm_SetInt0Callback(Scheduler_Int0Callback);
    Comm_SetInt1Callback(Scheduler_Int1Callback);
    Comm_SetInt2Callback(Scheduler_Int2Callback);
    Comm_SetInt3Callback(Scheduler_Int3Callback);

    /* 配置 BB UART RX 下降沿中断（检测起始位） */
    GPIO_SetISR(GPIO_PRT21, 3, GPIO_IRQ_EDGE_FALLING, NULL);

    Comm_InitInterrupts();  /* 使能中断 */

    /* 使能 GPIO 综合中断 NVIC（BB UART RX + J5 事件） */
    GPIO_Interrupt_Init();

    /* 初始化雷达代理（注册 H7 回传帧回调） */
    Radar_Init();

    NPU_Init();         /* NPU 硬件初始化 */

    /* 摄像头初始化（失败不阻塞，可在运行时重试�?*/
    int cam_ret = OV7675_Init();
    if (cam_ret == 0) {
        Logger_Print(LOG_INFO, "OV7675 camera ready.");
    } else {
        Logger_Print(LOG_WARN, "OV7675 init failed (%d), camera disabled", cam_ret);
    }

    Logger_Print(LOG_INFO, "System Init Complete (BB UART on P21 + J5 INT).");
}
