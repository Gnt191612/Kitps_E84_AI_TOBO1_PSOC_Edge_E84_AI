/**
 * @file    protocol.h
 * @brief   与84E/ESP32的通信协议解析、回调、发送
 *
 * 公开接口：
 *   Protocol_Init()
 *   Protocol_Register84ECallback()   - 注册处理84E结果帧的回调
 *   Protocol_RegisterESP32Callback() - 注册处理ESP32跟踪帧的回调
 *   Protocol_Send84ECommand()        - 将命令打包为帧并发送给84E
 *   Protocol_SendESP32Command()      - 将命令打包为帧并发送给ESP32
 *   Protocol_ProcessIncoming()       - 在主循环中调用，处理接收队列
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 回调函数原型 */
typedef void (*Recv84ECallback_t)(uint8_t id, float conf,
                                  float x, float y, float dist, float angle);
typedef void (*RecvESP32Callback_t)(uint8_t id, float x, float y, uint8_t lost);

/* 浏览器指令回调：cmd_code(0x01=SWITCH, 0x02=TRACK, 0x03=RELEASE, 0x04=RELOAD), target_id */
typedef void (*RecvBrowserCmdCallback_t)(uint8_t cmd_code, uint8_t target_id);

void Protocol_Init(void);
void Protocol_Register84ECallback(Recv84ECallback_t cb);
void Protocol_RegisterESP32Callback(RecvESP32Callback_t cb);
void Protocol_RegisterBrowserCmdCallback(RecvBrowserCmdCallback_t cb);

void Protocol_Send84ECommand(uint8_t cmd_type, float angle, float width,
                             float minDist, float maxDist);
void Protocol_SendESP32Command(uint8_t esp_id, uint8_t cmd,
                               uint8_t target_id,
                               float param1, float param2);

/* 帧类型定义 */
#define FRAME_TYPE_UPLOAD       0x03   /* 文本上传（H7→ESP32→WiFi中继→浏览器） */
#define FRAME_TYPE_BROWSER_CMD  0x04   /* 浏览器指令转发（ESP32→H7）           */
void Protocol_SendESP32Text(uint8_t esp_id, const char *text, uint16_t len);

/* 供UART中断喂入字节 */
void Protocol_Feed84EByte(uint8_t byte);
void Protocol_FeedESP32Byte(uint8_t byte, uint8_t esp_id);

/* 主循环处理解析好的帧（调用注册的回调） */
void Protocol_ProcessIncoming(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */