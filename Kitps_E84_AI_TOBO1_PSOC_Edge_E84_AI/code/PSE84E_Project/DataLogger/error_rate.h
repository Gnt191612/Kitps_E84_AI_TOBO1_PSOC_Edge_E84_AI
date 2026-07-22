/**
 * @file    error_rate.h
 * @brief   识别错误率与跟丢率统计
 * 
 * 公开接口：
 *   ErrorRate_Init()            - 初始化统计结构
 *   ErrorRate_RecordRecognition() - 记录一次识别结果（正确/错误）
 *   ErrorRate_RecordTracking()    - 记录一次跟踪事件（成功/丢失）
 *   ErrorRate_GetRecognitionError() - 返回识别错误率（0~1）
 *   ErrorRate_GetTrackingLossRate() - 返回跟丢率（0~1）
 *   ErrorRate_PrintReport()       - 打印统计报告
 */

#ifndef __ERROR_RATE_H
#define __ERROR_RATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t total_recognitions;   /* 总识别次数 */
    uint32_t correct_recognitions; /* 正确识别次数 */
    uint32_t total_tracks;         /* 总跟踪帧数 */
    uint32_t lost_tracks;          /* 跟丢次数 */
} ErrorRate_t;

void     ErrorRate_Init(void);
void     ErrorRate_RecordRecognition(uint8_t is_correct);
void     ErrorRate_RecordTracking(uint8_t is_lost);
float    ErrorRate_GetRecognitionError(void);
float    ErrorRate_GetTrackingLossRate(void);
float    ErrorRate_GetCurrentRate(void);  /* 返回当前识别正确率（1 - error） */
void     ErrorRate_PrintReport(void);

#ifdef __cplusplus
}
#endif

#endif /* __ERROR_RATE_H */