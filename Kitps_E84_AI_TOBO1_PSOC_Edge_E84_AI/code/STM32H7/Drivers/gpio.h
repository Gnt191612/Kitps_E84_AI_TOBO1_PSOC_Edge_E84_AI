/**
 * @file    gpio.h
 * @brief   通用IO控制（LED、复位、片选等）
 *
 * 公开接口：
 *   GPIO_Init()        - 初始化（由CubeMX调用）
 *   GPIO_SetLED(uint8_t state)      - 控制板载LED（0:灭, 1:亮）
 *   GPIO_ToggleLED()                 - 翻转LED
 *   GPIO_Reset84E(void)             - 硬件复位84E板
 */

#ifndef __GPIO_H
#define __GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void GPIO_SetLED(uint8_t state);
void GPIO_ToggleLED(void);
void GPIO_Reset84E(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H */