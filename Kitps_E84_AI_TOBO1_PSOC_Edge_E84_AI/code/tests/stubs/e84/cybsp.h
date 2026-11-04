/* 测试替身：BSP 中 h7_link.c 用到的资源名 */
#ifndef TEST_STUB_CYBSP_H
#define TEST_STUB_CYBSP_H
#include "cy_pdl.h"
#define CYBSP_TENXER_UART_HW            (&stub_scb1)
#define CYBSP_TENXER_UART_config        stub_uart_cfg
#define CYBSP_SERIAL_INT_2_PORT         (&stub_prt9)
#define CYBSP_SERIAL_INT_2_PIN          2U
#define CYBSP_SERIAL_INT_2_config       stub_pin_cfg
#define CYBSP_SERIAL_INT_3_PORT         (&stub_prt9)
#define CYBSP_SERIAL_INT_3_PIN          3U
#define CYBSP_SERIAL_INT_3_config       stub_pin_cfg
#define peri_0_group_8_div_16_0_GRP_NUM 0
#define peri_0_group_8_div_16_0_HW      0
#define peri_0_group_8_div_16_0_NUM     0
#endif
