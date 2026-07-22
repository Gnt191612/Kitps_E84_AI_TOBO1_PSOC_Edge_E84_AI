/**
 * @file    performance.c
 * @brief   性能统计实现
 * 
 * 使用系统滴答定时器扩展微秒级计时（通过 DWT 或 TIM 实现，此处用 HAL_GetTick 粗精度）。
 * 若需微秒精度，可基于 DWT->CYCCNT 实现，但代码保持简洁。
 */

#include "performance.h"
#include "logger.h"
#include <string.h>

/* 外部声明：如果需要微秒计数，可调用 DWT 初始化函数 */
extern uint32_t GetMicros(void);   /* 用户需实现，返回系统启动以来的微秒数 */

static Performance_t g_perf;

void Perf_Init(void)
{
    memset(&g_perf, 0, sizeof(g_perf));
}

void Perf_StartTimer(void)
{
    g_perf.last_start_us = GetMicros();
}

void Perf_StopTimer(uint32_t *elapsed_us)
{
    uint32_t now = GetMicros();
    *elapsed_us = now - g_perf.last_start_us;
}

void Perf_RecordNPUInference(uint32_t elapsed_us)
{
    if (g_perf.inference_count < PERF_MAX_SAMPLES) {
        g_perf.inference_times_us[g_perf.inference_count++] = elapsed_us;
    }
}

void Perf_RecordScanTime(uint32_t elapsed_us)
{
    if (g_perf.scan_count < PERF_MAX_SAMPLES) {
        g_perf.scan_times_us[g_perf.scan_count++] = elapsed_us;
    }
}

uint32_t Perf_GetAvgInference_us(void)
{
    if (g_perf.inference_count == 0) return 0;
    uint64_t sum = 0;
    for (int i = 0; i < g_perf.inference_count; i++) {
        sum += g_perf.inference_times_us[i];
    }
    return (uint32_t)(sum / g_perf.inference_count);
}

uint32_t Perf_GetAvgScanTime_us(void)
{
    if (g_perf.scan_count == 0) return 0;
    uint64_t sum = 0;
    for (int i = 0; i < g_perf.scan_count; i++) {
        sum += g_perf.scan_times_us[i];
    }
    return (uint32_t)(sum / g_perf.scan_count);
}

void Perf_PrintReport(void)
{
    Logger_Print(LOG_INFO, "==== Performance Report ====");
    Logger_Print(LOG_INFO, "NPU Inference: avg %lu us (%d samples)",
                 Perf_GetAvgInference_us(), g_perf.inference_count);
    Logger_Print(LOG_INFO, "Scan Time    : avg %lu us (%d samples)",
                 Perf_GetAvgScanTime_us(), g_perf.scan_count);
}