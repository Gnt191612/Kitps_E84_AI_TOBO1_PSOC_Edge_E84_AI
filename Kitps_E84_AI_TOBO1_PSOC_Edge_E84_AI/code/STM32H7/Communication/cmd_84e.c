/**
 * @file    cmd_84e.c
 * @brief   封装对84E的协议发送
 */

#include "cmd_84e.h"
#include "protocol.h"
#include "logger.h"

void Cmd_84E_Init(void)
{
    Logger_Print(LOG_INFO, "84E Command Interface Ready.");
}

void Cmd_84E_SendScanCmd(uint8_t target_id, float center_angle, float width,
                         float min_dist, float max_dist)
{
    Protocol_Send84ECommand(0, target_id, center_angle, width, min_dist, max_dist);
}

/* 命令类型和 84E 的 h7_link.h 一致：2 开始连续扫描，3 停止 */
void Cmd_84E_SendScanTrackCmd(uint8_t target_id, float center_angle, float width,
                              float min_dist, float max_dist)
{
    Protocol_Send84ECommand(2, target_id, center_angle, width, min_dist, max_dist);
}

void Cmd_84E_SendStopCmd(uint8_t target_id)
{
    Protocol_Send84ECommand(3, target_id, 0.0f, 0.0f, 0.0f, 0.0f);
}
