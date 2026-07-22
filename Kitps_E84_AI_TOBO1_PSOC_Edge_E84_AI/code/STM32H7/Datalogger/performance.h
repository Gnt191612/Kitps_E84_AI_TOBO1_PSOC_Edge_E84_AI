/**
 * @file    performance.h
 * @brief   高性能计时器接口（基于 DWT_CYCCNT）
 *
 * 公开接口：
 *   Perf_Init()                    - 使能 DWT 循环计数器
 *   Perf_StartTimer()              - 记录起始计数值
 *   Perf_StopTimer(elapsed_us)     - 停止计时，返回经过微秒数
 *   Perf_RecordNPUInference(us)    - 记录一次 NPU 推理耗时
 *   Perf_GetAvgNPUInference()      - 获取 NPU 推理平均耗时
 *   Perf_PrintNPUStats()           - 打印 NPU 推理统计到日志
 *
 * 依赖：
 *   - STM32H7 HAL 库（CoreDebug, DWT）
 *   - main.h（huart1 声明用于 Logger_Print）
 */

#ifndef __PERFORMANCE_H
#define __PERFORMANCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/
void     Perf_Init(void);
void     Perf_StartTimer(void);
uint32_t Perf_StopTimer(uint32_t *elapsed_us);
void     Perf_RecordNPUInference(uint32_t elapsed_us);
uint32_t Perf_GetAvgNPUInference(void);
void     Perf_PrintNPUStats(void);

#ifdef __cplusplus
}
#endif

#endif /* __PERFORMANCE_H */
