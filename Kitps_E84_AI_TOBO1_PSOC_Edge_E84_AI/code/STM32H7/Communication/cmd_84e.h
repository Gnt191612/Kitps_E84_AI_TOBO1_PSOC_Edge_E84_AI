/**
 * @file    cmd_84e.h
 * @brief   向84E下发识别/扫描指令
 *
 * 公开接口：
 *   Cmd_84E_Init()
 *   Cmd_84E_SendScanCmd()   - 请求84E在指定窗口识别一帧
 *   Cmd_84E_SendScanTrackCmd() - 开始边扫描边跟踪：84E 每帧回传置信度和目标在画面中的位置
 *   Cmd_84E_SendStopCmd()   - 结束边扫描边跟踪
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
void Cmd_84E_SendScanTrackCmd(uint8_t target_id, float center_angle, float width,
                              float min_dist, float max_dist);
void Cmd_84E_SendStopCmd(uint8_t target_id);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_84E_H */
