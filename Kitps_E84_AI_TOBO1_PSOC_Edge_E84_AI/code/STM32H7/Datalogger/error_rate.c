/**
 * @file    error_rate.c
 * @brief   错误率统计实现
 * 
 * 使用全局计数器，线程安全需由调用者保证（通常在单线程中断或任务中调用）。
 */

#include "error_rate.h"
#include "logger.h"

static ErrorRate_t g_error;

void ErrorRate_Init(void)
{
    g_error.total_recognitions = 0;
    g_error.correct_recognitions = 0;
    g_error.total_tracks = 0;
    g_error.lost_tracks = 0;
}

void ErrorRate_RecordRecognition(uint8_t is_correct)
{
    g_error.total_recognitions++;
    if (is_correct) {
        g_error.correct_recognitions++;
    }
}

void ErrorRate_RecordTracking(uint8_t is_lost)
{
    g_error.total_tracks++;
    if (is_lost) {
        g_error.lost_tracks++;
    }
}

float ErrorRate_GetRecognitionError(void)
{
    if (g_error.total_recognitions == 0) return 0.0f;
    return (float)(g_error.total_recognitions - g_error.correct_recognitions)
           / (float)g_error.total_recognitions;
}

float ErrorRate_GetTrackingLossRate(void)
{
    if (g_error.total_tracks == 0) return 0.0f;
    return (float)g_error.lost_tracks / (float)g_error.total_tracks;
}

void ErrorRate_PrintReport(void)
{
    Logger_Print(LOG_INFO, "==== Error Rate Report ====");
    Logger_Print(LOG_INFO, "Recognition: total=%lu, correct=%lu, error rate=%.2f%%",
                 g_error.total_recognitions, g_error.correct_recognitions,
                 ErrorRate_GetRecognitionError() * 100.0f);
    Logger_Print(LOG_INFO, "Tracking   : total=%lu, lost=%lu, loss rate=%.2f%%",
                 g_error.total_tracks, g_error.lost_tracks,
                 ErrorRate_GetTrackingLossRate() * 100.0f);
}