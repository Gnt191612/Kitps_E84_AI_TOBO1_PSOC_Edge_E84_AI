/**
 * @file protocol.h
 * @brief 协议状态机接收、帧解析、发送结果
 *
 * 与 H7 通信协议的状态机实现。
 * H7 → ESP32 指令: type=0x02, payload=[cmd:1][target_id:1][param1:4][param2:4]
 * ESP32 → H7 结果: type=0x02,
 * payload=[target_id:1][x_mm:4][y_mm:4][pan_ctrl:4][tilt_ctrl:4][lost:1]
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ──── H7 指令定义 (cmd) ──── */
#define CMD_TRACK          0x10   /* 跟踪命令: param1=角度(°), param2=距离(cm) */
#define CMD_ASSIST_POS     0x11   /* 辅助定位命令 */
#define CMD_RELOCK         0x12   /* 复锁命令 */
#define CMD_RELEASE        0x13   /* 释放进程命令 */
#define CMD_UPLOAD_DATA    0x14   /* 上传文本到上位机 (H7→ESP32→WebSocket) */

/* ──── 丢失状态 ──── */
#define LOST_NORMAL        0      /* 正常跟踪 */
#define LOST_TARGET        1      /* 目标已丢失 */

/* ──── 结果帧负载字段偏移 ──── */
#define RESULT_TARGET_ID_OFF   0
#define RESULT_X_OFF           1
#define RESULT_Y_OFF           5
#define RESULT_PAN_CTRL_OFF    9
#define RESULT_TILT_CTRL_OFF   13
#define RESULT_LOST_OFF        17
#define RESULT_PAYLOAD_LEN     18

/* ──── 状态机状态 ──── */
typedef enum {
    PROTO_STATE_IDLE = 0,        /* 等待帧头 AA */
    PROTO_STATE_HEADER1,         /* 已收 AA */
    PROTO_STATE_LENGTH,          /* 已收 55, 等待 length */
    PROTO_STATE_TYPE,            /* 已收 length, 等待 type */
    PROTO_STATE_PAYLOAD,         /* 已收 type, 等待 payload */
    PROTO_STATE_XOR              /* 已收 payload, 等待 xor */
} ProtoState_t;

/* ──── 解析结果回调 ──── */
typedef void (*Proto_CmdCallback_t)(uint8_t cmd, uint8_t target_id,
                                     int32_t param1, int32_t param2);
typedef void (*Proto_UploadCallback_t)(const uint8_t *data, uint16_t len);

/* ──── 上传文本的帧类型 ──── */
#define FRAME_TYPE_UPLOAD   0x03   /* H7→ESP32 文本上传帧 */

/* ──── 协议句柄 ──── */
typedef struct {
    ProtoState_t state;
    uint8_t buf[FRAME_MAX_PAYLOAD_LEN + 4];  /* 接收缓冲区 */
    uint16_t buf_pos;
    uint8_t expected_length;                  /* 期望的负载总长 */
    uint8_t expected_payload_len;             /* 期望的 payload 长度 */
    Proto_CmdCallback_t cmd_callback;         /* 指令回调 */
    Proto_UploadCallback_t upload_callback;   /* 文本上传回调 */
} Protocol_t;

/* ──── 公共接口 ──── */

/**
 * @brief 初始化协议状态机
 * @param proto 协议句柄指针
 * @param cb    指令回调函数 (可为 NULL)
 */
void Protocol_Init(Protocol_t *proto, Proto_CmdCallback_t cb);
void Protocol_RegisterUploadCallback(Proto_UploadCallback_t cb);

/**
 * @brief 逐字节喂入接收数据（串口中断中调用）
 * @param proto 协议句柄
 * @param byte  接收到的字节
 */
void Protocol_ParseByte(Protocol_t *proto, uint8_t byte);

/**
 * @brief 发送跟踪结果到 H7
 * @param target_id 目标编号
 * @param x_mm      目标中心 X 坐标 (mm)
 * @param y_mm      目标中心 Y 坐标 (mm)
 * @param pan_ctrl  PID 水平控制量 (-400~400)
 * @param tilt_ctrl PID 垂直控制量 (-400~400)
 * @param lost      丢失标志 (0=正常, 1=丢失)
 */
void Protocol_SendTrackResult(uint8_t target_id, float x_mm, float y_mm,
                              float pan_ctrl, float tilt_ctrl, uint8_t lost);

/**
 * @brief 发送辅助定位信息
 * @param data 辅助定位数据指针
 * @param len  数据长度
 */
void Protocol_SendAssistPos(const uint8_t *data, uint8_t len);

/**
 * @brief 获取协议全局句柄
 */
Protocol_t *Protocol_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
