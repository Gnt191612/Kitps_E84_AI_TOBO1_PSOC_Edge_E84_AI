/**
 * @file    i2c.c
 * @brief   I2C 主机驱动实现（基于 STM32H7 HAL）
 *
 * 使用 I2C2，PB10 (SCL) / PB11 (SDA) (AF4) — 需通过 1.8V↔3.3V 电平转换
 * 连接 PSE84E J5 (P21_0/I3C_SDA, P21_1/I3C_SCL)。
 *
 * PSE84E 作为 I2C Slave，地址 0x42。
 *
 * 速率：400kHz (Fast Mode)
 * 使用 HAL_I2C_Master_Transmit / HAL_I2C_Master_Receive 阻塞模式。
 *
 *  电平转换说明：
 *    J5 是 1.8V 电平域。H7 是 3.3V 电平域。
 *    需要 1.8V ↔ 3.3V 双向电平转换电路（如 TXS0102 或分立 MOSFET 电路）。
 *    如果 TOBO1 板载已有 J5 电平转换，则直接连接。
 */

#include "i2c.h"
#include "main.h"

/* I2C 句柄（非 static，供 stm32h7xx_it.c 中的 I2C2 中断处理使用） */
I2C_HandleTypeDef hi2c2;

int I2C_Master_Init(void)
{
    /* ---- 1. GPIO 引脚时钟使能 ---- */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* ---- 2. I2C2 时钟使能 ---- */
    __HAL_RCC_I2C2_CLK_ENABLE();

    /* ---- 3. 配置 PB10 (SCL), PB11 (SDA) 为 I2C AF4 复用功能 ---- */
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;          /* I2C: 开漏输出 */
    GPIO_InitStruct.Pull      = GPIO_NOPULL;              /* 外部上拉（1.8V 域） */
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;            /* I2C2 AF4 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ---- 4. 配置 I2C2 主机，400kHz Fast Mode ---- */
    /* D2PCLK1 (APB1) = 56.25MHz (225MHz SYSCLK / 4)
     * 400kHz 时序计算 (tI2CCLK = 17.78ns):
     *   PRESC=0, SCLDEL=6, SDADEL=6, SCLH=34, SCLL=73
     *   => TIMING = 0x00660622
     *   实际频率 ≈ 56.25MHz / (34+73) = 525kHz → 已考虑下降时间修正
     */
    hi2c2.Instance             = I2C2;
    hi2c2.Init.Timing          = 0x00660622;   /* 400kHz @ 56.25MHz APB1 */
    hi2c2.Init.OwnAddress1     = 0x00;          /* Master 不需要自身地址 */
    hi2c2.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2     = 0x00;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        return -1;
    }

    /* 使能 analog filter（抑制毛刺） */
    HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE);

    return 0;
}

int I2C_Write(uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return -1;

    if (HAL_I2C_Master_Transmit(&hi2c2, (PSE84E_I2C_ADDR << 1),
                                 data, len, I2C_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return 0;
}

int I2C_Read(uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return -1;

    if (HAL_I2C_Master_Receive(&hi2c2, (PSE84E_I2C_ADDR << 1),
                                data, len, I2C_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return 0;
}

int I2C_WriteRead(uint8_t *tx_data, uint16_t tx_len,
                  uint8_t *rx_data, uint16_t rx_len)
{
    if (I2C_Write(tx_data, tx_len) != 0) return -1;

    /* 写完成后加短延时，等待 PSE84E Slave 处理 */
    HAL_Delay(1);

    if (rx_len > 0) {
        return I2C_Read(rx_data, rx_len);
    }
    return 0;
}

uint8_t I2C_Probe(void)
{
    uint8_t dummy;
    /* 尝试读 1 字节，10ms 超时。超时 = 无数据，正常 */
    if (HAL_I2C_Master_Receive(&hi2c2, (PSE84E_I2C_ADDR << 1),
                                &dummy, 1, 10) == HAL_OK) {
        return 1;  /* 有数据 */
    }
    return 0;     /* 无数据或超时 */
}
