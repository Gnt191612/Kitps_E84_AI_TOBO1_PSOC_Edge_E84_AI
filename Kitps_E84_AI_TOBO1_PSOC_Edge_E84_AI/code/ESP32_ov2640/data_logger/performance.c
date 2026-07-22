/**
 * @file performance.c
 * @brief 性能计时器
 *
 * 提供简单的高精度计时功能，用于测量各环节耗时。
 */

#include "data_logger/logger.h"
#include "esp_timer.h"
#include <string.h>

/* ──── 计时器结构 ──── */
#define PERF_MAX_TIMERS  8

typedef struct {
    const char *name;
    uint64_t start_time;           /* 开始时间 (us) */
    uint64_t total_elapsed;        /* 累计时间 (us) */
    uint64_t min_elapsed;
    uint64_t max_elapsed;
    int call_count;
    int running;                   /* 1=正在计时 */
} PerfTimer_t;

static PerfTimer_t s_timers[PERF_MAX_TIMERS];
static int s_timer_count = 0;

/**
 * @brief 创建计时器
 * @param name 计时器名称
 * @return 计时器 ID, -1=失败
 */
int Perf_Create(const char *name)
{
    if (s_timer_count >= PERF_MAX_TIMERS) return -1;

    int id = s_timer_count++;
    s_timers[id].name = name;
    s_timers[id].start_time = 0;
    s_timers[id].total_elapsed = 0;
    s_timers[id].min_elapsed = 0xFFFFFFFFFFFFFFFFULL;
    s_timers[id].max_elapsed = 0;
    s_timers[id].call_count = 0;
    s_timers[id].running = 0;
    return id;
}

/**
 * @brief 开始计时
 * @param id 计时器 ID
 */
void Perf_Start(int id)
{
    if (id < 0 || id >= s_timer_count) return;
    s_timers[id].start_time = esp_timer_get_time();
    s_timers[id].running = 1;
}

/**
 * @brief 停止计时并记录
 * @param id 计时器 ID
 * @return 本次耗时 (us)
 */
uint64_t Perf_Stop(int id)
{
    if (id < 0 || id >= s_timer_count || !s_timers[id].running) return 0;

    uint64_t end = esp_timer_get_time();
    uint64_t elapsed = end - s_timers[id].start_time;

    s_timers[id].total_elapsed += elapsed;
    s_timers[id].call_count++;
    s_timers[id].running = 0;

    if (elapsed < s_timers[id].min_elapsed) s_timers[id].min_elapsed = elapsed;
    if (elapsed > s_timers[id].max_elapsed) s_timers[id].max_elapsed = elapsed;

    return elapsed;
}

/**
 * @brief 打印所有计时器统计
 */
void Perf_PrintAll(void)
{
    LOG_INFO("==== Performance Timers ====");
    for (int i = 0; i < s_timer_count; i++) {
        if (s_timers[i].call_count > 0) {
            uint64_t avg = s_timers[i].total_elapsed / s_timers[i].call_count;
            LOG_INFO("  %s: calls=%d, avg=%llu us, min=%llu us, max=%llu us",
                     s_timers[i].name,
                     s_timers[i].call_count,
                     (unsigned long long)avg,
                     (unsigned long long)s_timers[i].min_elapsed,
                     (unsigned long long)s_timers[i].max_elapsed);
        }
    }
    LOG_INFO("============================");
}

/**
 * @brief 重置指定计时器
 * @param id 计时器 ID
 */
void Perf_Reset(int id)
{
    if (id < 0 || id >= s_timer_count) return;
    s_timers[id].total_elapsed = 0;
    s_timers[id].min_elapsed = 0xFFFFFFFFFFFFFFFFULL;
    s_timers[id].max_elapsed = 0;
    s_timers[id].call_count = 0;
    s_timers[id].running = 0;
}

/**
 * @brief 重置所有计时器
 */
void Perf_ResetAll(void)
{
    for (int i = 0; i < s_timer_count; i++) {
        Perf_Reset(i);
    }
}
