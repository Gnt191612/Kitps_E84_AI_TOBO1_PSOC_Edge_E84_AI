/**
 * @file    exp_log.c
 * @brief   实验事件记录实现
 */

#include "exp_log.h"

#if EXP_LOG_ENABLE
#include "logger.h"
#include "main.h"

void ExpLog_Event(const char *event, uint8_t target_id,
                  int32_t v1, int32_t v2, int32_t v3)
{
    Logger_Raw("$E,%lu,%s,%u,%ld,%ld,%ld",
               (unsigned long)HAL_GetTick(), event, (unsigned)target_id,
               (long)v1, (long)v2, (long)v3);
}
#endif
