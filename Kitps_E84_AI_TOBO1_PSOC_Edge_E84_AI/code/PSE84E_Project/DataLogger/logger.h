/**
 * @file    logger.h
 * @brief   本地日志打印接口
 * 
 * 公开接口：
 *   Logger_Init()            - 初始化日志系统（重定向串口等）
 *   Logger_Print()           - 打印格式化字符串（类似 printf）
 *   Logger_LogRaw()          - 输出原始字节数组（Hex dump）
 *   Logger_SetLevel()        - 设置日志等级（0:关闭 1:错误 2:警告 3:信息 4:调试）
 */

#ifndef __LOGGER_H
#define __LOGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_OFF   = 0,
    LOG_ERROR = 1,
    LOG_WARN  = 2,
    LOG_INFO  = 3,
    LOG_DEBUG = 4
} LogLevel_t;

void Logger_Init(void);
void Logger_Print(LogLevel_t level, const char *fmt, ...);
void Logger_LogRaw(const uint8_t *data, uint16_t len);
void Logger_SetLevel(LogLevel_t level);

#ifdef __cplusplus
}
#endif

#endif /* __LOGGER_H */