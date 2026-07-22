/**
 * @file    network.c
 * @brief   网络通信存根实现
 *
 * 将目标切换等函数委托给 data_exchange 模块。
 */

#include "network.h"
#include "data_exchange.h"

void Network_SendTargetHandover(uint8_t target_id, float distance_cm)
{
    DataExchange_SendHandover(target_id, distance_cm);
}
