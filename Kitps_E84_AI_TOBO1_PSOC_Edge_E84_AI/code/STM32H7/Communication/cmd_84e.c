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

void Cmd_84E_SendScanCmd(float center_angle, float width,
                         float min_dist, float max_dist)
{
    Protocol_Send84ECommand(0, center_angle, width, min_dist, max_dist);
}