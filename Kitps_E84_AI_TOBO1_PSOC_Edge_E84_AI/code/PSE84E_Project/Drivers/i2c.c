/**
 * @file    i2c.c
 * @brief   I2C Slave 驱动实现（基于 Infineon PDL Cy_SCB_I2C）
 *
 * 使用 SCB3 配置为 I2C Slave，连接 J5 (P21_0/I3C_SDA, P21_1/I3C_SCL)。
 * STM32H7 作为 I2C Master 与之通信 (400kHz Fast Mode)。
 *
 * 通信方式：中断驱动（通过 Cy_SCB_I2C_SlaveInterrupt 处理）
 * 从机地址：0x42
 */

#include "i2c.h"
#include "gpio.h"
#include <string.h>

/* ======================================================================== */
/* 静态变量                                                                  */
/* ======================================================================== */

static cy_stc_scb_i2c_context_t g_i2cContext;
static uint8_t  g_writeBuf[I2C_BUF_SIZE];
static uint8_t  g_readBuf[I2C_BUF_SIZE];
static volatile uint16_t g_writeLen = 0;
static volatile uint16_t g_readLen = 0;
static void (*g_rxCallback)(uint8_t *data, uint16_t len) = NULL;
static uint8_t  g_i2cInitialized = 0;
static volatile uint8_t g_writePending = 0;

/* ======================================================================== */
/* 公开接口实现                                                              */
/* ======================================================================== */

int I2C_Slave_Init(GPIO_PRT_Type *sda_port, uint32_t sda_pin,
                   GPIO_PRT_Type *scl_port, uint32_t scl_pin,
                   uint8_t slave_addr)
{
    if (!sda_port || !scl_port) return -1;

    /* ---- 1. 配置 SDA/SCL 引脚 ---- */
    cy_stc_gpio_pin_config_t sdaConfig = {
        .outVal    = 1U,
        .driveMode = CY_GPIO_DM_OD_DRIVESLOW,
        .hsiom     = (en_hsiom_sel_t)I2C_SDA_HSIOM_SEL,
        .intEdge   = CY_GPIO_INTR_DISABLE,
        .intMask   = 0U,
        .vtrip     = CY_GPIO_VTRIP_CMOS,
        .slewRate  = CY_GPIO_SLEW_FAST,
        .driveSel  = 0U,
        .vregEn    = 0U,
        .ibufMode  = 0U,
        .vtripSel  = 0U,
        .vrefSel   = 0U,
        .vohSel    = 0U,
    };
    Cy_GPIO_Pin_Init(sda_port, sda_pin, &sdaConfig);

    cy_stc_gpio_pin_config_t sclConfig = {
        .outVal    = 1U,
        .driveMode = CY_GPIO_DM_OD_DRIVESLOW,
        .hsiom     = (en_hsiom_sel_t)I2C_SCL_HSIOM_SEL,
        .intEdge   = CY_GPIO_INTR_DISABLE,
        .intMask   = 0U,
        .vtrip     = CY_GPIO_VTRIP_CMOS,
        .slewRate  = CY_GPIO_SLEW_FAST,
        .driveSel  = 0U,
        .vregEn    = 0U,
        .ibufMode  = 0U,
        .vtripSel  = 0U,
        .vrefSel   = 0U,
        .vohSel    = 0U,
    };
    Cy_GPIO_Pin_Init(scl_port, scl_pin, &sclConfig);

    /* ---- 2. 配置 I2C Slave ---- */
    cy_stc_scb_i2c_config_t i2cConfig = {
        .i2cMode               = CY_SCB_I2C_SLAVE,
        .useRxFifo             = true,
        .useTxFifo             = true,
        .slaveAddress          = (uint32_t)slave_addr,
        .slaveAddressMask      = 0U,
        .acceptAddrInFifo      = true,
        .ackGeneralAddr        = false,
        .enableWakeFromSleep   = false,
        .lowPhaseDutyCycle     = 0U,
        .highPhaseDutyCycle    = 0U,
        .enableDigitalFilter   = false,
    };

    cy_en_scb_i2c_status_t status;
    status = Cy_SCB_I2C_Init(I2C_SCB, &i2cConfig, &g_i2cContext);
    if (status != CY_SCB_I2C_SUCCESS) return -1;

    /* ---- 3. 配置读写缓冲区 ---- */
    Cy_SCB_I2C_SlaveConfigWriteBuf(I2C_SCB, g_writeBuf, I2C_BUF_SIZE, &g_i2cContext);
    Cy_SCB_I2C_SlaveConfigReadBuf(I2C_SCB, g_readBuf, 0, &g_i2cContext);

    /* ---- 4. 使能 I2C ---- */
    Cy_SCB_I2C_Enable(I2C_SCB);

    /* ---- 5. 使能 NVIC 中的 SCB 中断 ---- */
    NVIC_SetPriority(I2C_SCB_IRQ, 2U);
    NVIC_EnableIRQ(I2C_SCB_IRQ);

    g_i2cInitialized = 1;
    g_writePending = 0;
    g_writeLen = 0;
    g_readLen = 0;

    return 0;
}

void I2C_Slave_Deinit(void)
{
    if (!g_i2cInitialized) return;

    Cy_SCB_I2C_Disable(I2C_SCB, &g_i2cContext);
    Cy_SCB_I2C_DeInit(I2C_SCB);
    g_i2cInitialized = 0;
}

void I2C_Slave_Poll(void)
{
    if (!g_i2cInitialized) return;

    /* 获取 Slave 状态 */
    uint32_t status = Cy_SCB_I2C_SlaveGetStatus(I2C_SCB, &g_i2cContext);

    if (status & CY_SCB_I2C_SLAVE_WRITE_EVENT) {
        /* 新鲜数据已写入 */
        Cy_SCB_I2C_SlaveClearWriteStatus(I2C_SCB, &g_i2cContext);

        g_writeLen = (uint16_t)Cy_SCB_I2C_SlaveGetWriteTransferCount(I2C_SCB, &g_i2cContext);
        if (g_writeLen > 0 && g_writeLen <= I2C_BUF_SIZE) {
            g_writePending = 1;
            if (g_rxCallback) {
                g_rxCallback(g_writeBuf, g_writeLen);
            }
        }
    }

    /* 检查读完成事件 */
    if (status & CY_SCB_I2C_SLAVE_RD_CMPLT) {
        Cy_SCB_I2C_SlaveClearReadStatus(I2C_SCB, &g_i2cContext);
    }
}

uint8_t *I2C_GetWriteBuf(uint16_t *out_len)
{
    if (out_len) *out_len = g_writeLen;
    return g_writeBuf;
}

uint8_t *I2C_GetReadBuf(void)
{
    return g_readBuf;
}

void I2C_CommitReadBuf(uint16_t len)
{
    if (len > I2C_BUF_SIZE) len = I2C_BUF_SIZE;
    g_readLen = len;
    Cy_SCB_I2C_SlaveConfigReadBuf(I2C_SCB, g_readBuf, g_readLen, &g_i2cContext);
}

uint8_t I2C_IsWritePending(void)
{
    return g_writePending;
}

void I2C_SetRxCallback(void (*cb)(uint8_t *data, uint16_t len))
{
    g_rxCallback = cb;
}

void I2C_IRQHandler(void)
{
    /* PDL 自带的 I2C slave 中断处理函数 */
    Cy_SCB_I2C_SlaveInterrupt(I2C_SCB, &g_i2cContext);

    /* 中断后立即轮询读取结果 */
    I2C_Slave_Poll();
}
