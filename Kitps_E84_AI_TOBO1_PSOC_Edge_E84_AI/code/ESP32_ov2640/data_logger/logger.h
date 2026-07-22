/**
 * @file logger.h
 * @brief 简单 UART 日志 (通过 ESP-IDF ESP_LOG 封装)
 *
 * 接口: Logger_Init, Logger_Print, LOG_INFO, LOG_WARN, LOG_ERROR
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 日志级别 ──── */
#define LOG_LEVEL_ERROR    1
#define LOG_LEVEL_WARN     2
#define LOG_LEVEL_INFO     3
#define LOG_LEVEL_DEBUG    4

/* ──── 便捷宏 ──── */
#define LOG_ERROR(fmt, ...)  Logger_Print(LOG_LEVEL_ERROR, "[ERROR] " fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   Logger_Print(LOG_LEVEL_WARN,  "[WARN]  " fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   Logger_Print(LOG_LEVEL_INFO,  "[INFO]  " fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)  Logger_Print(LOG_LEVEL_DEBUG, "[DEBUG] " fmt, ##__VA_ARGS__)

/* ──── 公共接口 ──── */

/**
 * @brief 初始化日志系统
 * @return 0=成功
 */
int Logger_Init(void);

/**
 * @brief 打印日志
 * @param level 日志级别
 * @param fmt   格式化字符串
 * @param ...   变参
 */
void Logger_Print(int level, const char *fmt, ...);

/**
 * @brief 设置日志级别过滤
 * @param level 最小输出级别
 */
void Logger_SetLevel(int level);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
