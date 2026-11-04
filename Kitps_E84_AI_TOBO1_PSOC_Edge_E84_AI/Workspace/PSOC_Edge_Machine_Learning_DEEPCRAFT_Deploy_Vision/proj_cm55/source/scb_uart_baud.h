/*****************************************************************************
* \file scb_uart_baud.h
*
* \brief
* Re-program an SCB UART clock divider for a given baud rate at run time, based
* on the divider's actual input clock (the BSP values are fixed per alias).
*
*******************************************************************************/
#ifndef _PROJ_CM55_SCB_UART_BAUD_H_
#define _PROJ_CM55_SCB_UART_BAUD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "cy_pdl.h"

/* Chooses oversample (8..16) and divider for 'baud', writes the divider and
 * cfg->oversample. Call before Cy_SCB_UART_Init(). Returns the resulting baud. */
uint32_t scb_uart_set_baud(cy_stc_scb_uart_config_t *cfg, en_clk_dst_t div_grp,
                           cy_en_divider_types_t div_type, uint32_t div_num, uint32_t baud);

#ifdef __cplusplus
}
#endif

#endif /* _PROJ_CM55_SCB_UART_BAUD_H_ */
