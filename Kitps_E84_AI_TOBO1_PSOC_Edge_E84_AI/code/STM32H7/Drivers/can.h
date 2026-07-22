/**
 * @file    can.h
 * @brief   CAN 总线预留扩展通信接口
 *
 * 公开接口：
 *   CAN_Init()                      - 初始化 CAN 外设
 *   CAN_SendMessage(uint32_t id, uint8_t *data, uint8_t len) - 发送标准帧/扩展帧
 *   CAN_RegisterRxCallback(void (*cb)(uint32_t id, uint8_t *data, uint8_t len)) - 注册接收回调
 */

#ifndef __CAN_H
#define __CAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CAN_Init(void);
void CAN_SendMessage(uint32_t id, uint8_t *data, uint8_t len);
void CAN_RegisterRxCallback(void (*cb)(uint32_t id, uint8_t *data, uint8_t len));

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H */