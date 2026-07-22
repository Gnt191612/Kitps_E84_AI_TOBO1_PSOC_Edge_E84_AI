/**
 * @file    spi.c
 * @brief   SPI 驱动实现（基于 Infineon PDL Cy_SCB_SPI）
 */

#include "spi.h"
#include "gpio.h"
#include <string.h>

static cy_stc_scb_spi_context_t s_spiCtx[9] = {0};  /* SCB0 ~ SCB8 */

int SPI_Init(CySCB_Type *scb, uint32_t freq)
{
    if (!scb) return -1;

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

    cy_stc_scb_spi_config_t spiConfig = {
        .spiMode                 = CY_SCB_SPI_MASTER,            /* Master 模式 */
        .subMode                 = CY_SCB_SPI_MOTOROLA,          /* Motorola 格式 */
        .sclkMode                = CY_SCB_SPI_CPHA0_CPOL0,       /* Mode 0 */
        .parity                  = CY_SCB_SPI_PARITY_NONE,
        .dropOnParityError       = false,
        .oversample              = 8UL,                          /* SCB 时钟分频 */
        .rxDataWidth             = 8UL,                          /* 8 位数据宽度 */
        .txDataWidth             = 8UL,
        .enableMsbFirst          = true,                         /* MSB first */
        .enableFreeRunSclk       = false,
        .enableInputFilter       = false,
        .enableMisoLateSample    = false,
        .enableTransferSeperation = false,
        .ssPolarity              = CY_SCB_SPI_ACTIVE_LOW,        /* CS 低有效 */
        .ssSetupDelay            = false,
        .ssHoldDelay             = false,
        .ssInterFrameDelay       = false,
        .enableWakeFromSleep     = false,
        .rxFifoTriggerLevel      = 0UL,
        .rxFifoIntEnableMask     = 0UL,
        .txFifoTriggerLevel      = 0UL,
        .txFifoIntEnableMask     = 0UL,
        .masterSlaveIntEnableMask = 0UL,
    };

    cy_en_scb_spi_status_t status;
    status = Cy_SCB_SPI_Init(scb, &spiConfig, &s_spiCtx[scbIdx]);
    if (status != CY_SCB_SPI_SUCCESS) return -1;

    Cy_SCB_SPI_Enable(scb);
    return 0;
}

int SPI_Transfer(CySCB_Type *scb, const uint8_t *txBuf, uint8_t *rxBuf, uint16_t len)
{
    if (!scb || !txBuf || len == 0) return -1;

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

    Cy_SCB_SPI_Transfer(scb, (void *)txBuf, rxBuf, len, &s_spiCtx[scbIdx]);
    while (Cy_SCB_SPI_GetNumInRxFifo(scb) < len);
    return 0;
}

int SPI_Transmit(CySCB_Type *scb, const uint8_t *data, uint16_t len)
{
    return SPI_Transfer(scb, data, NULL, len);
}

int SPI_Receive(CySCB_Type *scb, uint8_t *data, uint16_t len)
{
    if (!scb || !data || len == 0) return -1;

    /* 发送 0xFF 以产生时钟，同时接收 */
    uint8_t txDummy[64];
    uint16_t chunk;
    uint16_t offset = 0;

    while (offset < len) {
        chunk = (len - offset < 64) ? (len - offset) : 64;
        memset(txDummy, 0xFF, chunk);
        if (SPI_Transfer(scb, txDummy, &data[offset], chunk) != 0)
            return -1;
        offset += chunk;
    }
    return 0;
}
