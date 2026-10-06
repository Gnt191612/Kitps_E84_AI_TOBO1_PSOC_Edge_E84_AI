/**
 * @file    cmd_84e.h
 * @brief   向84E下发识别/扫描指令
 *
 * 公开接口：
 *   Cmd_84E_Init()
 *   Cmd_84E_SendScanCmd()   - 请求84E在指定窗口进行NPU识别
 */

#ifndef __CMD_84E_H
#define __CMD_84E_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Cmd_84E_Init(void);
void Cmd_84E_SendScanCmd(uint8_t target_id, float center_angle, float width,
                         float min_dist, float max_dist);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_84E_H */
