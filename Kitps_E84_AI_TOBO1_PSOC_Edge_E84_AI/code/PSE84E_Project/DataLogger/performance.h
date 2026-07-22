/**
 * @file    performance.h
 * @brief   NPU推理延迟与扫描效率统计
 * 
 * 公开接口：
 *   Perf_Init()                - 初始化性能计数器
 *   Perf_StartTimer()          - 记录起始时间（返回句柄）
 *   Perf_StopTimer()           - 停止计时并记录延迟（单位：微秒）
 *   Perf_RecordNPUInference()  - 快捷记录一次 NPU 推理耗时
 *   Perf_RecordScanTime()      - 记录扫描耗时
 *   Perf_GetAvgInference_us()  - 获取平均推理延迟
 *   Perf_GetAvgScanTime_us()   - 获取平均扫描耗时
 *   Perf_PrintReport()         - 通过日志输出性能报告
 */

#ifndef __PERFORMANCE_H
#define __PERFORMANCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PERF_MAX_SAMPLES  100   /* 统计最大样本数 */

typedef struct {
    uint32_t inference_times_us[PERF_MAX_SAMPLES];
    uint16_t inference_count;
    uint32_t scan_times_us[PERF_MAX_SAMPLES];
    uint16_t scan_count;
    uint32_t last_start_us;     /* 临时计时起点（微秒） */
} Performance_t;

void    Perf_Init(void);
void    Perf_StartTimer(void);                     /* 记录起始微秒数 */
void    Perf_StopTimer(uint32_t *elapsed_us);      /* 返回流逝微秒数 */
void    Perf_RecordNPUInference(uint32_t elapsed_us);
void    Perf_RecordScanTime(uint32_t elapsed_us);
uint32_t Perf_GetAvgInference_us(void);
uint32_t Perf_GetAvgScanTime_us(void);
void    Perf_PrintReport(void);

#ifdef __cplusplus
}
#endif

#endif /* __PERFORMANCE_H */