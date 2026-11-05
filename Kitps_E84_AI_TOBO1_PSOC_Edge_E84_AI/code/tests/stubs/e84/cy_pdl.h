/* 测试替身：英飞凌 PDL 中 h7_link.c 用到的最小子集 */
#ifndef TEST_STUB_CY_PDL_H
#define TEST_STUB_CY_PDL_H
#include <stdint.h>
#include <stdio.h>

typedef struct { int dummy; } CySCB_Type;
typedef struct { int dummy; } GPIO_PRT_Type;
typedef struct { int dummy; } cy_stc_gpio_pin_config_t;
typedef struct { uint32_t oversample; } cy_stc_scb_uart_config_t;
typedef struct { int dummy; } cy_stc_scb_uart_context_t;
typedef int en_clk_dst_t;
typedef int cy_en_divider_types_t;
typedef enum { CY_SCB_UART_SUCCESS = 0 } cy_en_scb_uart_status_t;

#define CY_SCB_UART_RX_OVERFLOW   0x01U
#define CY_SCB_UART_RX_ERR_FRAME  0x02U

extern CySCB_Type stub_scb1;
extern GPIO_PRT_Type stub_prt9;
extern const cy_stc_scb_uart_config_t stub_uart_cfg;
extern const cy_stc_gpio_pin_config_t stub_pin_cfg;

static inline int Cy_GPIO_Pin_Init(GPIO_PRT_Type *p, uint32_t pin, const cy_stc_gpio_pin_config_t *c)
{ (void)p; (void)pin; (void)c; return 0; }
static inline cy_en_scb_uart_status_t Cy_SCB_UART_Init(CySCB_Type *b, cy_stc_scb_uart_config_t const *c,
                                                       cy_stc_scb_uart_context_t *ctx)
{ (void)b; (void)c; (void)ctx; return CY_SCB_UART_SUCCESS; }
static inline void Cy_SCB_UART_Enable(CySCB_Type *b) { (void)b; }
static inline uint32_t Cy_SCB_UART_GetRxFifoStatus(CySCB_Type const *b) { (void)b; return 0; }
static inline void Cy_SCB_UART_ClearRxFifoStatus(CySCB_Type *b, uint32_t m) { (void)b; (void)m; }

/* 收发由测试驱动 */
void     Cy_SCB_UART_PutArrayBlocking(CySCB_Type *b, void *buf, uint32_t n);
uint32_t Cy_SCB_UART_GetNumInRxFifo(CySCB_Type const *b);
uint32_t Cy_SCB_UART_Get(CySCB_Type const *b);
#endif
