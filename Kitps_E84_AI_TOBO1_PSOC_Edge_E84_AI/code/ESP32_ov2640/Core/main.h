/**
 * @file main.h
 * @brief 主入口头文件
 *
 * 声明基础外设句柄和系统函数。
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 基础外设句柄 ──── */
/* (UART、I2C 等句柄在各驱动模块内部管理) */

/* ──── 系统函数 ──── */
void System_Init(void);
void Scheduler_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
