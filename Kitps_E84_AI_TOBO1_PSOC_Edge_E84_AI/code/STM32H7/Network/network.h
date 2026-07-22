/**
 * @file    network.h
 * @brief   网络通信接口（最小化存根）
 *
 * 提供目标切换等网络操作接口，实际实现委托给 data_exchange 模块。
 * 参见 data_exchange.h 中的 DataExchange_SendHandover()。
 */

#ifndef __NETWORK_H
#define __NETWORK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 发送目标切换请求到上位机
 * @param target_id   目标编号
 * @param distance_cm 目标距离(cm)
 */
void Network_SendTargetHandover(uint8_t target_id, float distance_cm);

#ifdef __cplusplus
}
#endif

#endif /* __NETWORK_H */
