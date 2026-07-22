/**
 * @file    tcp_client.h
 * @brief   TCP客户端管理
 *
 * 公开接口：
 *   TCP_Client_Init()           - 配置服务器IP并连接
 *   TCP_Client_IsConnected()    - 连接状态查询
 *   TCP_Client_Send()           - 发送数据
 */

#ifndef __TCP_CLIENT_H
#define __TCP_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TCP_Client_Init(void);
int  TCP_Client_IsConnected(void);
void TCP_Client_Send(uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __TCP_CLIENT_H */