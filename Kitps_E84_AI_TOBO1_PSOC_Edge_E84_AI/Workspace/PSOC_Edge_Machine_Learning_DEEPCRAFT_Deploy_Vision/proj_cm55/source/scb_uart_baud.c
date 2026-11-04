/******************************************************************************
* File Name: scb_uart_baud.c
*
* Description: Run-time SCB UART baud selection (shared by h7_link.c and
*              dataset_capture.c).
*
*******************************************************************************/
#include "scb_uart_baud.h"

uint32_t scb_uart_set_baud(cy_stc_scb_uart_config_t *cfg, en_clk_dst_t div_grp,
                           cy_en_divider_types_t div_type, uint32_t div_num, uint32_t baud)
{
    uint32_t cur_div = Cy_SysClk_PeriPclkGetDivider(div_grp, div_type, div_num);
    uint32_t f_in = Cy_SysClk_PeriPclkGetFrequency(div_grp, div_type, div_num) * (cur_div + 1U);
    uint32_t best_os = 8U, best_div = 1U, best_err = UINT32_MAX;

    for (uint32_t os = 8U; os <= 16U; os++)
    {
        uint32_t div = (f_in + (baud * os) / 2U) / (baud * os);
        if ((div == 0U) || (div > 65536U))
        {
            continue;
        }
        uint32_t actual = f_in / (div * os);
        uint32_t err = (actual > baud) ? (actual - baud) : (baud - actual);
        if (err < best_err)
        {
            best_err = err;
            best_os = os;
            best_div = div;
        }
    }

    Cy_SysClk_PeriPclkDisableDivider(div_grp, div_type, div_num);
    Cy_SysClk_PeriPclkSetDivider(div_grp, div_type, div_num, best_div - 1U);
    Cy_SysClk_PeriPclkEnableDivider(div_grp, div_type, div_num);
    cfg->oversample = best_os;

    return Cy_SysClk_PeriPclkGetFrequency(div_grp, div_type, div_num) / best_os;
}
