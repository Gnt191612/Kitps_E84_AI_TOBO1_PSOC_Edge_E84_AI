/**
 * @file logger.c
 * @brief 日志系统实现 (基于 ESP-IDF ESP_LOG)
 */

#include "logger.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "ESP32_OV2640";
static int s_min_level = LOG_LEVEL_INFO;

/* ──── 初始化 ──── */
int Logger_Init(void)
{
    /* ESP-IDF ESP_LOG 默认已初始化 */
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    ESP_LOGI(TAG, "Logger initialized, min_level=%d", s_min_level);
    return 0;
}

/* ──── 设置级别 ──── */
void Logger_SetLevel(int level)
{
    s_min_level = level;
}

/* ──── 打印日志 ──── */
void Logger_Print(int level, const char *fmt, ...)
{
    if (level > s_min_level) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    switch (level) {
    case LOG_LEVEL_ERROR:
        ESP_LOGE(TAG, "%s", buf);
        break;
    case LOG_LEVEL_WARN:
        ESP_LOGW(TAG, "%s", buf);
        break;
    case LOG_LEVEL_INFO:
        ESP_LOGI(TAG, "%s", buf);
        break;
    case LOG_LEVEL_DEBUG:
    default:
        ESP_LOGD(TAG, "%s", buf);
        break;
    }
}
