/**
 * @file    eth.h
 * @brief   以太网硬件驱动初始化接口
 *
 * 公开接口：
 *   ETH_Init() - 初始化ETH外设、lwIP协议栈、启动网口
 */

#ifndef __ETH_H
#define __ETH_H

#ifdef __cplusplus
extern "C" {
#endif

void ETH_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __ETH_H */