/**
 * @file    data_exchange.h
 * @brief   上位机数据交换协议
 *
 * 公开接口：
 *   DataExchange_Init()
 *   DataExchange_SendTargetList()      - 发送全部目标信息（JSON格式）
 *   DataExchange_SendHandover()        - 发送目标切换指令
 */

#ifndef __DATA_EXCHANGE_H
#define __DATA_EXCHANGE_H

#include <stdint.h>
#include "target_list.h"

#ifdef __cplusplus
extern "C" {
#endif

void DataExchange_Init(void);
void DataExchange_SendTargetList(TargetList_t *list);
void DataExchange_SendHandover(uint8_t target_id, float distance_cm);
void DataExchange_SendStats(float recog_error_rate, float loss_rate);

#ifdef __cplusplus
}
#endif

#endif /* __DATA_EXCHANGE_H */