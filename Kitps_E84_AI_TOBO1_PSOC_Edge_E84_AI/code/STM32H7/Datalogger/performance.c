/**
 * @file    performance.c
 * @brief   高性能计时器（基于 DWT_CYCCNT）
 *
 * 使用 Cortex-M7 数据观察点与跟踪单元 (DWT) 的 CYCCNT 寄存器
 * 实现微秒级计时。STM32H7 系列内置 DWT，无需额外外设。
 *
 * 公开接口：
 *   Perf_Init()                    - 使能 DWT 循环计数器
 *   Perf_StartTimer()              - 记录起始计数值
 *   Perf_StopTimer(elapsed_us)     - 计算并返回经过的微秒数
 *   Perf_RecordNPUInference(us)    - 记录一次 NPU 推理耗时
 *   Perf_GetAvgNPUInference()      - 获取 NPU 推理平均耗时
 *   Perf_PrintNPUStats()           - 打印 NPU 推理统计
 *
 * 依赖：
 *   - STM32H7 HAL 库（CoreDebug, DWT 相关寄存器）
 *   - main.h（获取系统时钟频率，非必需）
 *
 * 注意：
 *   DWT_CYCCNT 在调试暂停时也会暂停，因此测量的是实际运行周期。
 *   需确保 CoreDebug->DEMCR 的 TRCENA 位已置位。
 */

#include "performance.h"
#include "logger.h"
#include "main.h"
#include <string.h>

/*----------------------------------------------------------------------------
 * 内部常量
 *----------------------------------------------------------------------------*/
#define NPU_HISTORY_LEN     32          /* NPU 推理耗时历史记录数 */
#define TOTAL_HISTORY_LEN   16          /* 通用计时历史记录数 */

#ifndef CORE_CLOCK_MHZ
/* STM32H743 默认主频 480 MHz；可根据实际工程配置修改 */
#define CORE_CLOCK_MHZ      480.0f
#endif

/*----------------------------------------------------------------------------
 * 内部数据结构
 *----------------------------------------------------------------------------*/
typedef struct {
    uint32_t history[NPU_HISTORY_LEN];
    uint8_t  idx;
    uint8_t  count;
} NPUStats_t;

/*----------------------------------------------------------------------------
 * 静态变量
 *----------------------------------------------------------------------------*/
static uint32_t    g_startCycle = 0;
static uint32_t    g_totalHistory[TOTAL_HISTORY_LEN];
static uint8_t     g_totalIdx = 0;
static NPUStats_t  g_npuStats;

static uint8_t     g_initialized = 0;

/*----------------------------------------------------------------------------
 * 使能 DWT 循环计数器
 *----------------------------------------------------------------------------*/
void Perf_Init(void)
{
    /* 使能 DWT 和 ITM 的调试访问 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 复位并使能 CYCCNT */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    memset(&g_npuStats, 0, sizeof(g_npuStats));
    memset(g_totalHistory, 0, sizeof(g_totalHistory));
    g_startCycle = 0;
    g_totalIdx = 0;
    g_initialized = 1;
}

/*----------------------------------------------------------------------------
 * 记录起始时间
 *----------------------------------------------------------------------------*/
void Perf_StartTimer(void)
{
    if (!g_initialized) return;
    g_startCycle = DWT->CYCCNT;
}

/*----------------------------------------------------------------------------
 * 停止计时并返回经过的微秒数
 *
 * @param[out] elapsed_us  经过的微秒数（可为 NULL）
 * @return  经过的微秒数
 *----------------------------------------------------------------------------*/
uint32_t Perf_StopTimer(uint32_t *elapsed_us)
{
    if (!g_initialized) {
        if (elapsed_us) *elapsed_us = 0;
        return 0;
    }

    uint32_t cycles = DWT->CYCCNT - g_startCycle;

    /* 折算为微秒：cycles / MHz */
    uint32_t us = (uint32_t)((float)cycles / CORE_CLOCK_MHZ + 0.5f);
    if (us == 0 && cycles > 0) us = 1;

    /* 写入历史环状缓冲 */
    g_totalHistory[g_totalIdx] = us;
    g_totalIdx = (g_totalIdx + 1) % TOTAL_HISTORY_LEN;

    if (elapsed_us) {
        *elapsed_us = us;
    }

    return us;
}

/*----------------------------------------------------------------------------
 * 记录一次 NPU 推理耗时
 *
 * @param elapsed_us  NPU 推理所耗微秒数（外部测量或 Perf_StopTimer 结果）
 *----------------------------------------------------------------------------*/
void Perf_RecordNPUInference(uint32_t elapsed_us)
{
    g_npuStats.history[g_npuStats.idx] = elapsed_us;
    g_npuStats.idx = (g_npuStats.idx + 1) % NPU_HISTORY_LEN;
    if (g_npuStats.count < NPU_HISTORY_LEN) {
        g_npuStats.count++;
    }
}

/*----------------------------------------------------------------------------
 * 获取 NPU 推理平均耗时
 *
 * @return  平均微秒数，若无记录返回 0
 *----------------------------------------------------------------------------*/
uint32_t Perf_GetAvgNPUInference(void)
{
    if (g_npuStats.count == 0) return 0;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < g_npuStats.count; i++) {
        sum += g_npuStats.history[i];
    }
    return sum / g_npuStats.count;
}

/*----------------------------------------------------------------------------
 * 打印 NPU 推理耗时统计到日志
 *----------------------------------------------------------------------------*/
void Perf_PrintNPUStats(void)
{
    if (g_npuStats.count == 0) {
        Logger_Print(LOG_INFO, "[PERF] No NPU inference records.");
        return;
    }

    /* 计算最大值和最小值 */
    uint32_t min_us = g_npuStats.history[0];
    uint32_t max_us = g_npuStats.history[0];
    uint32_t sum    = 0;

    for (uint8_t i = 0; i < g_npuStats.count; i++) {
        uint32_t v = g_npuStats.history[i];
        sum += v;
        if (v < min_us) min_us = v;
        if (v > max_us) max_us = v;
    }

    uint32_t avg_us = sum / g_npuStats.count;

    Logger_Print(LOG_INFO, "[PERF] NPU Inference: avg=%lu us, min=%lu us, max=%lu us, samples=%u",
                 avg_us, min_us, max_us, g_npuStats.count);
}
