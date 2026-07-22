/**
 * @file    delay_record.c
 * @brief   延迟记录实现
 */

#include "delay_record.h"
#include "logger.h"
#include "main.h"

#define MAX_RECORDS 20

typedef struct {
    uint8_t  id;
    uint32_t start_tick;
    uint32_t elapsed;
} DelayEntry_t;

static DelayEntry_t g_delays[MAX_RECORDS];
static uint8_t g_delay_cnt = 0;

void DelayRecord_Start84E(uint8_t target_id)
{
    if (g_delay_cnt < MAX_RECORDS) {
        g_delays[g_delay_cnt].id = target_id;
        g_delays[g_delay_cnt].start_tick = HAL_GetTick();
        g_delay_cnt++;
    }
}

void DelayRecord_Stop84E(uint8_t target_id)
{
    for (int i = 0; i < g_delay_cnt; i++) {
        if (g_delays[i].id == target_id && g_delays[i].elapsed == 0) {
            g_delays[i].elapsed = HAL_GetTick() - g_delays[i].start_tick;
            break;
        }
    }
}

void DelayRecord_PrintStats(void)
{
    for (int i = 0; i < g_delay_cnt; i++) {
        if (g_delays[i].elapsed) {
            Logger_Print(LOG_INFO, "Target %d delay: %lu ms", g_delays[i].id, g_delays[i].elapsed);
        }
    }
}