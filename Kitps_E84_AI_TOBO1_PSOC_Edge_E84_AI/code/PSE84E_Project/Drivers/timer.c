/**
 * @file    timer.c
 * @brief   定时器驱动实现（基于 Infineon PDL Cy_TCPWM v2）
 *
 * 使用 Cy_TCPWM_TriggerStart_Single / Cy_TCPWM_TriggerStopOrKill_Single
 * 这些函数不受 CY_IP_MXS40TCPWM 条件编译保护，可用于所有 CAT1C 设备。
 */

#include "timer.h"
#include <string.h>

int Timer_Init(TCPWM_Type *tcpwm, uint32_t group, uint32_t period, uint32_t clockHz)
{
    if (!tcpwm) return -1;

    (void)clockHz;

    cy_stc_tcpwm_pwm_config_t pwmConfig = {
        .pwmMode            = CY_TCPWM_PWM_CONTINUOUS,
        .clockPrescaler     = CY_TCPWM_PWM_PRESCALER_DIVBY_1,
        .pwmAlignment       = CY_TCPWM_PWM_LEFT_ALIGN,
        .deadTimeClocks     = 0,
        .runMode            = CY_TCPWM_PWM_CONTINUOUS,
        .period0            = period,
        .period1            = 0,
        .enablePeriodSwap   = false,
        .compare0           = period / 2,
        .compare1           = 0,
        .enableCompareSwap  = false,
        .interruptSources   = 0UL,
        .invertPWMOut       = 0UL,
        .invertPWMOutN      = 0UL,
        .killMode           = 0U,
        .swapInputMode      = CY_TCPWM_INPUT_0,
        .swapInput          = 0UL,
        .reloadInputMode    = CY_TCPWM_INPUT_0,
        .reloadInput        = 0UL,
        .startInputMode     = CY_TCPWM_INPUT_0,
        .startInput         = 0UL,
        .killInputMode      = CY_TCPWM_INPUT_0,
        .killInput          = 0UL,
        .countInputMode     = CY_TCPWM_INPUT_0,
        .countInput         = 0UL,
        .swapOverflowUnderflow = false,
        .immediateKill      = false,
        .tapsEnabled        = 0U,
        .compare2           = 0,
        .compare3           = 0,
        .enableCompare1Swap = false,
        .compare0MatchUp    = false,
        .compare0MatchDown  = false,
        .compare1MatchUp    = false,
        .compare1MatchDown  = false,
        .kill1InputMode     = CY_TCPWM_INPUT_0,
        .kill1Input         = 0UL,
        .pwmOnDisable       = 0UL,
        .trigger0Event      = 0UL,
        .trigger1Event      = 0UL,
        .reloadLineSelect   = false,
        .line_out_sel       = CY_TCPWM_OUTPUT_PWM_SIGNAL,
        .linecompl_out_sel  = CY_TCPWM_OUTPUT_INVERTED_PWM_SIGNAL,
        .line_out_sel_buff  = CY_TCPWM_OUTPUT_PWM_SIGNAL,
        .linecompl_out_sel_buff = CY_TCPWM_OUTPUT_INVERTED_PWM_SIGNAL,
        .deadTimeClocks_linecompl_out = 0U,
    };

    cy_en_tcpwm_status_t status;
    status = Cy_TCPWM_PWM_Init(tcpwm, group, &pwmConfig);
    if (status != CY_TCPWM_SUCCESS) return -1;

    Cy_TCPWM_PWM_Enable(tcpwm, group);
    return 0;
}

void Timer_Start(TCPWM_Type *tcpwm, uint32_t group)
{
    if (!tcpwm) return;
    Cy_TCPWM_TriggerStart_Single(tcpwm, group);
}

void Timer_Stop(TCPWM_Type *tcpwm, uint32_t group)
{
    if (!tcpwm) return;
    Cy_TCPWM_TriggerStopOrKill_Single(tcpwm, group);
}
