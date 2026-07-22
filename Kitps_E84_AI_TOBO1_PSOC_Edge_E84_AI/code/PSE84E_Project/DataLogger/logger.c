/**
 * @file    logger.c
 * @brief   日志系统实现（串口输出）
 * 
 * 依赖：底层串口发送函数 UART_Transmit()（需外部实现，或重定向 printf）
 * 本实现使用重定向 printf 到串口，更简单。
 */

#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* 外部串口发送函数（需在 main.c 中实现） */
extern int _write(int fd, const char *ptr, int len); /* 如果使用 semihosting 或重定向 printf */

static LogLevel_t g_log_level = LOG_INFO;

/* 日志等级字符串 */
static const char *level_str[] = {
    "", "[ERR]", "[WRN]", "[INF]", "[DBG]"
};

void Logger_Init(void)
{
    /* 可在此配置串口，假设已由 CubeMX 初始化 */
    g_log_level = LOG_INFO;
}

void Logger_SetLevel(LogLevel_t level)
{
    g_log_level = level;
}

void Logger_Print(LogLevel_t level, const char *fmt, ...)
{
    if (level > g_log_level || level == LOG_OFF) return;

    /* 输出等级前缀 */
    printf("%s ", level_str[level]);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    /* 换行（可选） */
    printf("\r\n");
}

void Logger_LogRaw(const uint8_t *data, uint16_t len)
{
    if (g_log_level < LOG_DEBUG) return;

    printf("[RAW] ");
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");
}