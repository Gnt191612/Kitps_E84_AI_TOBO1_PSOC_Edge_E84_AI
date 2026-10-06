/**
 * @file    protocol.h
 * @brief   与H7的通信协议：指令解析、结果上报
 *
 * 公开接口：
 *   CommandPacket_t / ResultPacket_t      - 命令与结果结构体（坐标单位：cm，角度单位：度）
 *   Protocol_Init()                       - 初始化协议状态机
 *   Protocol_RegisterCommandCallback()    - 注册收到完整指令时的回调函数（通常为 Scheduler_PushCommand）
 *   Protocol_ParseByte(uint8_t byte)      - 串口字节输入，内部状态机解析
 *   Protocol_SendResult(ResultPacket_t *res) - 将结果封装成帧通过串口发送给H7
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 命令/结果数据包定义（与H7共同约定） ---------- */
#pragma pack(push, 1)   /* 确保结构体对齐为1字节，便于序列化 */

/* H7 下发给 84E 的指令 */
typedef struct {
    uint8_t  cmd_type;      /* 0:目标识别  1:辅助扫描 */
    uint8_t  target_id;     /* H7分配的目标编号（1~3） */
    float    center_angle;  /* 扫描中心角（度） */
    float    angle_width;   /* 扫描窗口宽度（度） */
    float    min_dist_cm;   /* 最小距离（cm） */
    float    max_dist_cm;   /* 最大距离（cm） */
    uint32_t timestamp_ms;  /* 指令时间戳 */
} CommandPacket_t;

/* 84E 回报给 H7 的识别结果（精简版，共 21 字节）
 * detected 由 H7 侧通过 confidence > 0 隐式判断
 * timestamp 由 H7 侧 SysTick 本地记录
 * ──────────────────────────────────────────
 * 偏移 | 字段        | 类型      | 说明
 *   0   | target_id  | uint8_t   | 目标编号（1~3）
 *   1-4 | confidence | float     | 置信度 0~1
 *   5-8 | x_cm       | float     | 目标 X 坐标（cm）
 *  9-12 | y_cm       | float     | 目标 Y 坐标（cm）
 * 13-16 | dist_cm    | float     | 径向距离（cm）
 * 17-20 | angle_deg  | float     | 角度（度）
 */
typedef struct {
    uint8_t  target_id;      /* 目标编号（1~3） */
    float    confidence;     /* 置信度 0~1，>0 视为有目标 */
    float    x_cm;           /* 目标 X 坐标（cm） */
    float    y_cm;           /* 目标 Y 坐标（cm） */
    float    dist_cm;        /* 径向距离（cm） */
    float    angle_deg;      /* 角度（度） */
} ResultPacket_t;           /* sizeof = 1 + 4*5 = 21 字节 */

#pragma pack(pop)

/* ---------- 协议控制函数 ---------- */
void Protocol_Init(void);
void Protocol_RegisterCommandCallback(void (*callback)(CommandPacket_t *cmd));
void Protocol_RegisterFrameCallback(void (*callback)(uint8_t type, uint8_t *payload, uint16_t len));
void Protocol_ParseByte(uint8_t byte);
void Protocol_SendResult(ResultPacket_t *res);
void Protocol_CheckIncoming(void);  /* 检查 I2C 是否有新命令到达 */

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */
