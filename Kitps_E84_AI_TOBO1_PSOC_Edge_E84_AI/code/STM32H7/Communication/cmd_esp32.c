/**
 * @file    cmd_esp32.c
 * @brief   封装对ESP32的协议发送及回调注册
 */

#include "cmd_esp32.h"
#include "protocol.h"
#include "logger.h"

void Cmd_ESP32_Init(void)
{
    Logger_Print(LOG_INFO, "ESP32 Command Interface Ready.");
}

void Cmd_ESP32_SendTrackCmd(uint8_t esp_id, float angle, float distance, uint8_t target_id)
{
    Protocol_SendESP32Command(esp_id, 0x10, target_id, angle, distance);
}

void Cmd_ESP32_RegisterTrackCallback(TrackResultCallback_t cb)
{
    /* 将回调转换为protocol层需要的格式 */
    // Protocol_RegisterESP32Callback 期望 void (*)(uint8_t id, float x, float y, uint8_t lost)
    Protocol_RegisterESP32Callback(cb);
}