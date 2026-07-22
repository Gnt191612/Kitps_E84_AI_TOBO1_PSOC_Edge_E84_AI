/**
 * @file    protocol.c
 * @brief   协议实现：字节接收状态机，拆帧，回调上层
 *
 * 内部状态：
 *   等待帧头1 → 等待帧头2 → 读取长度 → 读取类型+负载+校验 → 校验成功则序列化为 CommandPacket_t 并回调
 */

#include "protocol.h"
#include "frame.h"
#include "communication.h"
#include "i2c.h"
#include <string.h>
#include "logger.h"

/* 协议接收状态机 */
typedef enum {
    STATE_HEADER1,
    STATE_HEADER2,
    STATE_LENGTH,
    STATE_DATA
} ProtocolState_t;

static ProtocolState_t g_state = STATE_HEADER1;
static uint8_t         g_rx_buffer[FRAME_MAX_PAYLOAD + 4];  /* 暂存完整帧 */
static uint16_t        g_rx_index = 0;
static uint8_t         g_expected_len = 0;

/* 命令回调函数指针 */
static void (*g_cmd_callback)(CommandPacket_t *) = NULL;

/* 通用帧回调函数指针（用于非命令帧，如雷达回传等） */
static void (*g_frame_callback)(uint8_t type, uint8_t *payload, uint16_t len) = NULL;

/* 内部使用帧类型常量 */
#define FRAME_TYPE_COMMAND 0x01
#define FRAME_TYPE_RESULT  0x02

void Protocol_Init(void)
{
    g_state = STATE_HEADER1;
    g_rx_index = 0;
    Comm_SetRxCallback(Protocol_ParseByte);  /* 将解析函数注册为串口接收回调 */
}

void Protocol_RegisterCommandCallback(void (*callback)(CommandPacket_t *cmd))
{
    g_cmd_callback = callback;
}

void Protocol_RegisterFrameCallback(void (*callback)(uint8_t type, uint8_t *payload, uint16_t len))
{
    g_frame_callback = callback;
}

void Protocol_ParseByte(uint8_t byte)
{
    switch (g_state) {
    case STATE_HEADER1:
        if (byte == FRAME_HEADER1) {
            g_rx_buffer[0] = byte;
            g_rx_index = 1;
            g_state = STATE_HEADER2;
        }
        break;

    case STATE_HEADER2:
        if (byte == FRAME_HEADER2) {
            g_rx_buffer[1] = byte;
            g_rx_index = 2;
            g_state = STATE_LENGTH;
        } else {
            g_state = STATE_HEADER1;  /* 帧头错误，复位 */
        }
        break;

    case STATE_LENGTH:
        /* 长度字段：type+payload 的字节数 */
        if (byte <= FRAME_MAX_PAYLOAD && byte >= 1) {
            g_rx_buffer[2] = byte;
            g_expected_len = byte + 3;  /* 还要接收 type+payload+checksum，但已经包含 type，还需 payload 和 checksum */
            /* 注意：帧结构为 帧头2 + 长度1 + (type1 + payload_n + checksum1) = 总字节数 = 4 + payload_n */
            /* 这里长度字段 = 1 (type) + payload_len，所以还需接收的字节数 = 长度字段 + 1 (checksum) */
            g_rx_index = 3;
            g_state = STATE_DATA;
        } else {
            g_state = STATE_HEADER1;
        }
        break;

    case STATE_DATA:
        g_rx_buffer[g_rx_index++] = byte;
        /* 已接收长度字段后的全部数据：type + payload + checksum */
        if (g_rx_index >= (uint16_t)(4 + g_rx_buffer[2])) {  /* 4 = 两个帧头 + 长度字段 + 类型？ 不对，重新计算： */
            /* 帧头(2) + 长度(1) + 后续(长度字段值+1) = 总帧长 */
            /* g_rx_index 当前指向下一个要写入的位置，若已接收满则解析 */
            uint16_t total_len = 3 + g_rx_buffer[2] + 1; /* 3 = 帧头2+长度1，再加长度字段指示的(type+payload) + checksum */
            if (g_rx_index >= total_len) {
                /* 调用帧解析 */
                uint8_t type;
                uint8_t payload[FRAME_MAX_PAYLOAD];
                uint16_t payload_len;
                if (Frame_Unpack(g_rx_buffer, total_len, &type, payload, &payload_len) == 0) {
                    if (type == FRAME_TYPE_COMMAND) {
                        /* 将 payload 转换为 CommandPacket_t */
                        CommandPacket_t cmd;
                        if (payload_len == sizeof(CommandPacket_t)) {
                            memcpy(&cmd, payload, sizeof(CommandPacket_t));
                            if (g_cmd_callback) {
                                g_cmd_callback(&cmd);
                            }
                        } else {
                            Logger_Print(LOG_WARN, "Command size mismatch");
                        }
                    } else if (g_frame_callback) {
                        /* 非命令帧分发给通用帧回调（如雷达回传数据） */
                        g_frame_callback(type, payload, payload_len);
                    }
                    /* 其他未注册的帧类型直接忽略 */
                }
                /* 复位状态机 */
                g_state = STATE_HEADER1;
                g_rx_index = 0;
            }
        }
        break;

    default:
        g_state = STATE_HEADER1;
        break;
    }
}

void Protocol_CheckIncoming(void)
{
    /* 轮询 I2C 从机接收缓冲 */
    I2C_Slave_Poll();
}

void Protocol_SendResult(ResultPacket_t *res)
{
    uint8_t frame_buf[128];
    uint16_t frame_len;

    /* 将 ResultPacket_t 作为 payload 打包 */
    if (Frame_Pack(frame_buf, &frame_len, FRAME_TYPE_RESULT, (uint8_t*)res, sizeof(ResultPacket_t)) == 0) {
        Comm_SendData(frame_buf, frame_len);
    }
}