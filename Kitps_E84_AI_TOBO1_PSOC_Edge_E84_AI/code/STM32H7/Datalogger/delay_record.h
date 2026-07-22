/**
 * @file    delay_record.h
 * @brief   各环节延迟记录
 *
 * 公开接口：
 *   DelayRecord_Start84E()   - 记录向84E发送指令时的起始时间
 *   DelayRecord_Stop84E()    - 收到回复时计算延迟并存储
 *   DelayRecord_GetStats()   - 获取统计信息
 */

#ifndef __DELAY_RECORD_H
#define __DELAY_RECORD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void DelayRecord_Start84E(uint8_t target_id);
void DelayRecord_Stop84E(uint8_t target_id);
void DelayRecord_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* __DELAY_RECORD_H */