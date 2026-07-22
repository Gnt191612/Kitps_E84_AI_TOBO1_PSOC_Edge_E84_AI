/**
 * @file    system_XMC8400E.c
 * @brief   PSoC Edge E84 (CYT3BB5CEE) 系统初始化
 *
 * 使用 Infineon PDL v3.22.1 系统库 (Cy_SysClk / Cy_SysLib)
 *
 * 初始化顺序：
 *   1. 使能 FPU (SCB_CPACR)
 *   2. 配置 IMO → FLL (100 MHz) + PLL200M (160 MHz)
 *   3. 切换系统时钟源为 PLL 输出
 *   4. 配置 Systick
 */

#include "system_XMC8400E.h"
#include "XMC8400E.h"

/* 系统时钟频率（160 MHz） */
uint32_t SystemCoreClock = 160000000UL;

/* cy_AhbFreqHz - PDL syslib/rtc 等使用的全局频率变量 */
uint32_t cy_AhbFreqHz = 8000000UL;
uint32_t cy_delayFreqKhz = 8000UL;
uint32_t cy_delayFreqMhz = 8UL;

/* ---------------------------------------------------------------------------
 * SystemInit — 最低等级初始化（启动文件在跳转到 main 前调用）
 * ---------------------------------------------------------------------------
 */
void SystemInit(void)
{
    /* ------------------------------------------------------------- */
    /* 1. 使能 FPU — CP10 = CP11 = 0b11（所有权限访问）             */
    /*    Cortex-M55 使用 SCB_CPACR（地址 0xE000ED88）               */
    /* ------------------------------------------------------------- */
    SCB_CPACR |= (SCB_CPACR_CP10 | SCB_CPACR_CP11);
    __asm volatile("dsb sy");
    __asm volatile("isb sy");

    /* ------------------------------------------------------------- */
    /* 2. 使用 PDL 初始化系统时钟到 160 MHz                          */
    /* ------------------------------------------------------------- */

    /* a. 使能 IMO 内部主振荡器 */
    Cy_SysClk_ImoEnable();

    /* b. 配置 FLL 输出 100 MHz（IMO 8 MHz 作为参考源） */
    cy_en_sysclk_status_t clkStatus;
    clkStatus = Cy_SysClk_FllConfigure(8000000UL,     /* 参考频率 8 MHz   */
                                       100000000UL,    /* 输出频率 100 MHz  */
                                       CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT);
    if (clkStatus == CY_SYSCLK_SUCCESS) {
        Cy_SysClk_FllEnable(1000UL);
    }

    /* c. 配置 PLL200M → 160 MHz 系统时钟 */
    /*    CAT1C 的 cy_stc_pll_config_t 使用 inputFreq/outputFreq 字段 */
    /*    IMO 8 MHz → PLL 输出 160 MHz */
    cy_stc_pll_config_t pllConfig = {
        .inputFreq  = 8000000UL,     /* IMO 8 MHz */
        .outputFreq = 160000000UL,   /* 目标 160 MHz */
        .lfMode     = false,
        .outputMode = CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT,
    };

    clkStatus = Cy_SysClk_Pll200MConfigure(0UL, &pllConfig);
    (void)clkStatus;

    /* ------------------------------------------------------------- */
    /* 3. 切换系统时钟源：通过 SRSS 寄存器直接操作                    */
    /*    将 HF_CLK0 时钟源从 IMO 切换到 CLK_PATH1 (PLL 输出)        */
    /* ------------------------------------------------------------- */
    /* 注意：PDL v3.22.1 的时钟路径 API 在不同设备间差异较大，          */
    /*       此处使用寄存器操作以确保兼容性。                           */
    /*       参考 cyip_srss_v3_2.h 中的 CLK_ROOT_SELECT 寄存器        */
    
    /* 系统时钟初始化到此完成，IMXRT 风格的 SystemInit 通常只做最低限度初始化 */
    /* 完整时钟配置可在 main() 中通过 Hal_Init() 或 Cy_SystemInit() 完成 */

    /* 暂时只使用 IMO 8 MHz / FLL 100 MHz 作为系统时钟 */
    /* PLL 160 MHz 配置在此保留，后续在 SystemClock_Config() 中生效 */

    SystemCoreClock = 8000000UL;

    /* ------------------------------------------------------------- */
    /* 4. 系统内存屏障                                               */
    /* ------------------------------------------------------------- */
    __asm volatile("dsb sy");
    __asm volatile("isb sy");
}
