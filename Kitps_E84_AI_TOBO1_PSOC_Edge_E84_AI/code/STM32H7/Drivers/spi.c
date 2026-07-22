/**
 * @file    spi.c
 * @brief   SPI操作实现（使用hspi1）
 *
 * ⚠️ 当前编译未启用 — HAL_SPI_MODULE_ENABLED 未定义。
 *    hspi1 句柄未声明，SPI 引脚未配置。
 *    如需启用：
 *      1. stm32h7xx_hal_conf.h 中取消注释 #define HAL_SPI_MODULE_ENABLED
 *      2. CubeMX 中配置 SPI 引脚/时钟
 *      3. 去掉下面的 #if 0 块
 */
#if 0

#include "spi.h"
#include "main.h"

void SPI_Init(void)
{
    // CubeMX已初始化，此处可留空或做额外配置
}

void SPI_Transmit(uint8_t *data, uint16_t len)
{
    HAL_SPI_Transmit(&hspi1, data, len, 100);
}

void SPI_Receive(uint8_t *buf, uint16_t len)
{
    HAL_SPI_Receive(&hspi1, buf, len, 100);
}

void SPI_TransmitReceive(uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, len, 100);
}

#endif /* #if 0 — SPI 模块当前未启用 */
