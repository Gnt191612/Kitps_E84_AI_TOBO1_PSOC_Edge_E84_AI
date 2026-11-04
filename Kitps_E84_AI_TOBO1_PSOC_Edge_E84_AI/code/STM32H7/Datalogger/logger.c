/**
 * @file    logger.c
 * @brief   UART 日志输出实现（通过 huart1 发送）
 *
 * 实现方式：
 *   1. vsnprintf 格式化消息到内部缓冲区
 *   2. 通过 HAL_UART_Transmit 从 huart1 发送
 *   3. 每条消息以 "\r\n" 结尾
 *
 * 日志格式：[LEVEL] message\r\n
 *   等级前缀: D=DEBUG, I=INFO, W=WARN, E=ERROR
 */

#include "logger.h"
#include "main.h"               /* huart4 (UART4 调试串口) */

/* 日志专用串口。原先使用 huart2，会把文本混进 ESP32-A 的协议链路。 */
#define LOGGER_UART          (&huart4)
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * 内部常量
 *----------------------------------------------------------------------------*/
#define LOGGER_BUF_SIZE      256         /* 日志行最大长度 */
#define LOGGER_TIMEOUT_MS    100         /* UART 发送超时 */

/*----------------------------------------------------------------------------
 * 静态变量
 *----------------------------------------------------------------------------*/
static LogLevel_t g_minLevel = LOGGER_LEVEL_INFO;   /* 默认 INFO 及以上 */
static char       g_buffer[LOGGER_BUF_SIZE];

/* 等级到单字符前缀 */
static const char g_levelChar[] = {
    'D',    /* DEBUG */
    'I',    /* INFO  */
    'W',    /* WARN  */
    'E'     /* ERROR */
};

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void Logger_Init(void)
{
    g_minLevel = LOGGER_LEVEL_INFO;
    memset(g_buffer, 0, sizeof(g_buffer));
}

/*----------------------------------------------------------------------------
 * 设置日志过滤等级（低于该等级的消息将被丢弃）
 *----------------------------------------------------------------------------*/
void Logger_SetLevel(LogLevel_t level)
{
    if (level <= LOGGER_LEVEL_ERROR) {
        g_minLevel = level;
    }
}

/*----------------------------------------------------------------------------
 * 打印日志
 *
 * 如果 level < g_minLevel，消息被丢弃。
 * 格式化后通过 UART4 调试串口发送。
 *----------------------------------------------------------------------------*/
void Logger_Print(LogLevel_t level, const char *format, ...)
{
    va_list args;

    /* 等级过滤 */
    if (level > LOGGER_LEVEL_ERROR)      level = LOGGER_LEVEL_ERROR;
    if (level < g_minLevel) return;

    /* 格式化 */
    va_start(args, format);
    int len = vsnprintf(g_buffer, LOGGER_BUF_SIZE, format, args);
    va_end(args);

    if (len < 0) {
        /* vsnprintf 出错 */
        return;
    }

    /* 截断 */
    if (len >= LOGGER_BUF_SIZE) {
        len = LOGGER_BUF_SIZE - 1;
    }
    g_buffer[len] = '\0';

    /* 构造带等级的完整输出行 */
    /* 格式: [L] message\r\n */
    char out_buf[LOGGER_BUF_SIZE + 8];
    int out_len = snprintf(out_buf, sizeof(out_buf),
                           "[%c] %s\r\n",
                           g_levelChar[level], g_buffer);

    if (out_len > 0) {
        if (out_len >= (int)sizeof(out_buf)) {
            out_len = (int)sizeof(out_buf) - 1;
        }
        HAL_UART_Transmit(LOGGER_UART, (uint8_t *)out_buf, out_len,
                          LOGGER_TIMEOUT_MS);
    }
}

/*----------------------------------------------------------------------------
 * 原样输出一行（不加等级前缀、不受等级过滤），供实验数据记录使用
 *----------------------------------------------------------------------------*/
void Logger_Raw(const char *format, ...)
{
    va_list args;
    char out_buf[LOGGER_BUF_SIZE];

    va_start(args, format);
    int len = vsnprintf(out_buf, sizeof(out_buf) - 2, format, args);
    va_end(args);
    if (len < 0) return;
    if (len > (int)sizeof(out_buf) - 3) len = (int)sizeof(out_buf) - 3;
    out_buf[len++] = '\r';
    out_buf[len++] = '\n';
    HAL_UART_Transmit(LOGGER_UART, (uint8_t *)out_buf, len, LOGGER_TIMEOUT_MS);
}
