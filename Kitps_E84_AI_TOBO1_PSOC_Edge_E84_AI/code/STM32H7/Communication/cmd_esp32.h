/**
 * @file    cmd_esp32.h
 * @brief   向ESP32下发跟踪/复锁/辅助定位指令
 *
 * 公开接口：
 *   Cmd_ESP32_Init()
 *   Cmd_ESP32_SendTrackCmd()         - 启动或更新跟踪目标
 *   Cmd_ESP32_RegisterTrackCallback() - 注册接收ESP32跟踪结果的回调
 */

#ifndef __CMD_ESP32_H
#define __CMD_ESP32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 跟踪结果回调：ESP32编号、目标编号、坐标、是否丢失 */
typedef void (*TrackResultCallback_t)(uint8_t esp_id, uint8_t id,
                                      float x, float y,
                                      float pan_ctrl, float tilt_ctrl,
                                      uint8_t lost);

void Cmd_ESP32_Init(void);
void Cmd_ESP32_SendTrackCmd(uint8_t esp_id, float angle, float distance, uint8_t target_id);
void Cmd_ESP32_SendReleaseCmd(uint8_t esp_id);
void Cmd_ESP32_RegisterTrackCallback(TrackResultCallback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_ESP32_H */
