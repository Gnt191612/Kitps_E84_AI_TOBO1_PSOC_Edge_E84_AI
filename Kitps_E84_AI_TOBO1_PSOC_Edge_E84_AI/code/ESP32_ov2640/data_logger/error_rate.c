/**
 * @file error_rate.c
 * @brief 记录识别前后延迟和错误率
 *
 * 接口: ErrorRate_Record, ErrorRate_GetStats
 */

#include "data_logger/logger.h"
#include <string.h>
#include <stdio.h>

/* ──── 统计结构 ──── */
#define ERROR_RATE_WINDOW  100

typedef struct {
    int total_frames;
    int lost_frames;
    int success_frames;
    float total_latency_ms;      /* 累计延迟 */
    float min_latency_ms;
    float max_latency_ms;

    /* 滑动窗口 */
    uint32_t timestamps[ERROR_RATE_WINDOW];
    int window_head;
    int window_count;
} ErrorRate_t;

static ErrorRate_t s_er;

/**
 * @brief 初始化错误率统计
 */
void ErrorRate_Init(void)
{
    memset(&s_er, 0, sizeof(ErrorRate_t));
    s_er.min_latency_ms = 1e6f;
    s_er.max_latency_ms = 0;
    LOG_INFO("ErrorRate initialized");
}

/**
 * @brief 记录一次跟踪结果
 * @param lost     0=成功, 1=丢失
 * @param latency  该帧处理延迟 (ms)
 */
void ErrorRate_Record(int lost, float latency_ms)
{
    s_er.total_frames++;
    s_er.total_latency_ms += latency_ms;

    if (lost) {
        s_er.lost_frames++;
    } else {
        s_er.success_frames++;
    }

    if (latency_ms < s_er.min_latency_ms) s_er.min_latency_ms = latency_ms;
    if (latency_ms > s_er.max_latency_ms) s_er.max_latency_ms = latency_ms;

    /* 滑动窗口记录时间戳 */
    s_er.timestamps[s_er.window_head] = (uint32_t)(latency_ms * 1000);  /* us */
    s_er.window_head = (s_er.window_head + 1) % ERROR_RATE_WINDOW;
    if (s_er.window_count < ERROR_RATE_WINDOW) s_er.window_count++;
}

/**
 * @brief 获取统计信息
 * @param out_lost_rate   输出丢失率
 * @param out_avg_latency 输出平均延迟 (ms)
 * @param out_min_latency 输出最小延迟 (ms)
 * @param out_max_latency 输出最大延迟 (ms)
 */
void ErrorRate_GetStats(float *out_lost_rate, float *out_avg_latency,
                        float *out_min_latency, float *out_max_latency)
{
    if (s_er.total_frames > 0) {
        if (out_lost_rate)   *out_lost_rate   = (float)s_er.lost_frames / s_er.total_frames;
        if (out_avg_latency) *out_avg_latency = s_er.total_latency_ms / s_er.total_frames;
    } else {
        if (out_lost_rate) *out_lost_rate = 0;
        if (out_avg_latency) *out_avg_latency = 0;
    }
    if (out_min_latency) *out_min_latency = (s_er.min_latency_ms > 1e5f) ? 0 : s_er.min_latency_ms;
    if (out_max_latency) *out_max_latency = s_er.max_latency_ms;
}

/**
 * @brief 打印统计信息到日志
 */
void ErrorRate_PrintStats(void)
{
    float lost_rate, avg_lat, min_lat, max_lat;
    ErrorRate_GetStats(&lost_rate, &avg_lat, &min_lat, &max_lat);

    LOG_INFO("==== ErrorRate Stats ====");
    LOG_INFO("Total frames: %d", s_er.total_frames);
    LOG_INFO("Lost frames:  %d (%.1f%%)", s_er.lost_frames, lost_rate * 100);
    LOG_INFO("Success:      %d (%.1f%%)", s_er.success_frames, (1 - lost_rate) * 100);
    LOG_INFO("Avg latency:  %.2f ms",     avg_lat);
    LOG_INFO("Min latency:  %.2f ms",     min_lat);
    LOG_INFO("Max latency:  %.2f ms",     max_lat);
    LOG_INFO("========================");
}
