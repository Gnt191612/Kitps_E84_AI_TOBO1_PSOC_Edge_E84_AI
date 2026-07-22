/**
 * @file    logger.h
 * @brief   UART 日志输出（通过 huart2 发送到 ESP32-A）
 *
 * 公开接口：
 *   Logger_Init()                    - 初始化日志模块
 *   Logger_Print(level, format, ...) - 按日志等级输出格式化字符串
 *   Logger_SetLevel(level)           - 设置日志过滤等级
 *
 * 日志等级（LOGGER_LEVEL_xxx）：
 *   LOGGER_LEVEL_DEBUG = 0  - 调试信息
 *   LOGGER_LEVEL_INFO  = 1  - 普通信息（默认）
 *   LOGGER_LEVEL_WARN  = 2  - 警告
 *   LOGGER_LEVEL_ERROR = 3  - 错误（始终输出）
 *
 * 依赖：
 *   - main.h（声明 huart1）
 *   - STM32H7 HAL 库（HAL_UART_Transmit）
 */

#ifndef __LOGGER_H
#define __LOGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 日志等级枚举
 *----------------------------------------------------------------------------*/
typedef enum {
    LOGGER_LEVEL_DEBUG = 0,
    LOGGER_LEVEL_INFO  = 1,
    LOGGER_LEVEL_WARN  = 2,
    LOGGER_LEVEL_ERROR = 3
} LogLevel_t;

/* 为方便使用，提供简短别名兼容已有代码 */
#ifndef LOG_DEBUG
#define LOG_DEBUG   LOGGER_LEVEL_DEBUG
#endif
#ifndef LOG_INFO
#define LOG_INFO    LOGGER_LEVEL_INFO
#endif
#ifndef LOG_WARN
#define LOG_WARN    LOGGER_LEVEL_WARN
#endif
#ifndef LOG_ERROR
#define LOG_ERROR   LOGGER_LEVEL_ERROR
#endif

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/
void Logger_Init(void);
void Logger_Print(LogLevel_t level, const char *format, ...);
void Logger_SetLevel(LogLevel_t level);

#ifdef __cplusplus
}
#endif

#endif /* __LOGGER_H */
