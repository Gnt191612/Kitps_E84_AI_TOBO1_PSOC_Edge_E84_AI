/**
 * @file protocol.c
 * @brief 协议状态机接收、帧解析、发送结果实现
 */

#include "protocol.h"
#include "frame.h"
#include "Drivers/uart/uart.h"
#include <string.h>
#include <stdio.h>

/* 全局协议句柄 */
static Protocol_t s_proto;

/* ──── 初始化 ──── */
void Protocol_Init(Protocol_t *proto, Proto_CmdCallback_t cb)
{
    if (!proto) proto = &s_proto;

    proto->state = PROTO_STATE_IDLE;
    proto->buf_pos = 0;
    proto->expected_length = 0;
    proto->expected_payload_len = 0;
    proto->cmd_callback = cb;
    proto->upload_callback = NULL;
    memset(proto->buf, 0, sizeof(proto->buf));

    s_proto = *proto;  /* 保存到全局 */
}

void Protocol_RegisterUploadCallback(Proto_UploadCallback_t cb)
{
    s_proto.upload_callback = cb;
}

/* ──── 重置状态机 ──── */
static inline void reset_state(Protocol_t *proto)
{
    proto->state = PROTO_STATE_IDLE;
    proto->buf_pos = 0;
    proto->expected_length = 0;
    proto->expected_payload_len = 0;
}

/* ──── 完成一帧后的处理 ──── */
static void frame_complete(Protocol_t *proto)
{
    /* buf[0]=length, buf[1]=type, buf[2..]=payload */
    uint8_t total_len = proto->buf[0];
    uint8_t type = proto->buf[1];
    uint8_t pay_len = (total_len > 0) ? total_len - 1 : 0;

    /* 验证 XOR */
    uint8_t xor_received = proto->buf[2 + pay_len];           /* 最后1字节是 xor */
    uint8_t xor_calc = Frame_CalcXor(&proto->buf[1], 1 + pay_len); /* type+payload */

    if (xor_received != xor_calc) {
        reset_state(proto);
        return;
    }

    /* 处理 H7 指令 (type=0x02) */
    if (type == FRAME_TYPE_CMD && pay_len >= 10) {
        uint8_t cmd       = proto->buf[2];                             /* cmd */
        uint8_t target_id = proto->buf[3];                             /* target_id */
        int32_t param1;
        memcpy(&param1, &proto->buf[4], sizeof(int32_t));               /* param1 LE */
        int32_t param2;
        memcpy(&param2, &proto->buf[8], sizeof(int32_t));               /* param2 LE */

        if (proto->cmd_callback) {
            proto->cmd_callback(cmd, target_id, param1, param2);
        }
    }

    /* 处理 H7 文本上传 (type=0x03) */
    if (type == FRAME_TYPE_UPLOAD && pay_len > 0) {
        if (proto->upload_callback) {
            proto->upload_callback(&proto->buf[2], pay_len);
        }
    }

    reset_state(proto);
}

/* ──── 逐字节解析 ──── */
void Protocol_ParseByte(Protocol_t *proto, uint8_t byte)
{
    if (!proto) proto = &s_proto;

    switch (proto->state) {
    case PROTO_STATE_IDLE:
        if (byte == FRAME_HEADER1) {
            proto->state = PROTO_STATE_HEADER1;
        }
        break;

    case PROTO_STATE_HEADER1:
        if (byte == FRAME_HEADER2) {
            proto->state = PROTO_STATE_LENGTH;
        } else if (byte == FRAME_HEADER1) {
            /* 连续 AA, 仍然等待 55 */
        } else {
            reset_state(proto);
        }
        break;

    case PROTO_STATE_LENGTH:
        if (byte >= 1 && byte <= FRAME_MAX_PAYLOAD_LEN) {
            proto->expected_length = byte;
            proto->expected_payload_len = (byte > 0) ? byte - 1 : 0;
            proto->buf[0] = byte;   /* 存储 length */
            proto->buf_pos = 1;
            proto->state = PROTO_STATE_TYPE;
        } else {
            reset_state(proto);
        }
        break;

    case PROTO_STATE_TYPE:
        proto->buf[proto->buf_pos++] = byte;   /* type */
        proto->state = PROTO_STATE_PAYLOAD;
        break;

    case PROTO_STATE_PAYLOAD: {
        proto->buf[proto->buf_pos++] = byte;    /* payload */
        uint8_t total_stored = proto->buf_pos;  /* type + payload 已存字节数 */
        uint8_t need_total = 1 + proto->expected_payload_len;  /* type + payload */
        if (total_stored >= need_total) {
            proto->state = PROTO_STATE_XOR;
        }
        break;
    }

    case PROTO_STATE_XOR:
        proto->buf[proto->buf_pos++] = byte;    /* xor */
        frame_complete(proto);
        break;

    default:
        reset_state(proto);
        break;
    }
}

/* ──── 发送跟踪结果 ──── */
void Protocol_SendTrackResult(uint8_t target_id, float x_mm, float y_mm,
                              float pan_ctrl, float tilt_ctrl, uint8_t lost)
{
    uint8_t payload[RESULT_PAYLOAD_LEN];
    uint8_t buf[FRAME_OVERHEAD + RESULT_PAYLOAD_LEN];

    memset(payload, 0, sizeof(payload));
    payload[RESULT_TARGET_ID_OFF] = target_id;
    {
        int32_t x_int = (int32_t)(x_mm);
        memcpy(&payload[RESULT_X_OFF], &x_int, sizeof(int32_t));
    }
    {
        int32_t y_int = (int32_t)(y_mm);
        memcpy(&payload[RESULT_Y_OFF], &y_int, sizeof(int32_t));
    }
    {
        int32_t pan_int = (int32_t)(pan_ctrl);
        memcpy(&payload[RESULT_PAN_CTRL_OFF], &pan_int, sizeof(int32_t));
    }
    {
        int32_t tilt_int = (int32_t)(tilt_ctrl);
        memcpy(&payload[RESULT_TILT_CTRL_OFF], &tilt_int, sizeof(int32_t));
    }
    payload[RESULT_LOST_OFF] = lost;

    uint16_t frame_len = Frame_Pack(FRAME_TYPE_RESULT, payload, RESULT_PAYLOAD_LEN, buf);
    UART_Send(UART_NUM_1, buf, frame_len);
}

/* ──── 发送辅助定位 ──── */
void Protocol_SendAssistPos(const uint8_t *data, uint8_t len)
{
    uint8_t buf[FRAME_OVERHEAD + 32];
    uint16_t frame_len = Frame_Pack(FRAME_TYPE_RESULT, data, len, buf);
    UART_Send(UART_NUM_1, buf, frame_len);
}

/* ──── 获取全局句柄 ──── */
Protocol_t *Protocol_GetHandle(void)
{
    return &s_proto;
}
