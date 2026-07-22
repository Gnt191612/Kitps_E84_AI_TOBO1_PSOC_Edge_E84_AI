/**
 * @file frame.c
 * @brief 通信帧打包/解包实现
 */

#include "frame.h"
#include <string.h>

/* ──── XOR 校验 ──── */
uint8_t Frame_CalcXor(const uint8_t *data, uint8_t len)
{
    uint8_t xor_val = 0;
    for (uint8_t i = 0; i < len; i++) {
        xor_val ^= data[i];
    }
    return xor_val;
}

/* ──── 打包 ──── */
uint16_t Frame_Pack(uint8_t type, const uint8_t *payload, uint8_t pay_len, uint8_t *buf)
{
    uint8_t total_len = 1 + pay_len;   /* type + payload */

    buf[0] = FRAME_HEADER1;            /* AA */
    buf[1] = FRAME_HEADER2;            /* 55 */
    buf[2] = total_len;                /* length */
    buf[3] = type;                     /* type */

    if (payload && pay_len > 0) {
        memcpy(&buf[4], payload, pay_len);
    }

    /* XOR 从 type 到 payload 末尾（与 H7/PSE84E frame.c 一致） */
    buf[4 + pay_len] = Frame_CalcXor(&buf[3], 1 + pay_len);

    return FRAME_OVERHEAD + pay_len;
}

/* ──── 解包 ──── */
int Frame_Unpack(const uint8_t *buf, uint16_t len, Frame_t *out)
{
    if (!buf || !out) return -1;
    if (len < FRAME_OVERHEAD) return -1;

    if (buf[0] != FRAME_HEADER1 || buf[1] != FRAME_HEADER2) return -1;

    uint8_t total_len = buf[2];
    uint16_t frame_len = FRAME_OVERHEAD + (total_len - 1); /* total_len 包含 type, payload 部分 */

    if (len < frame_len) return -2;

    out->header[0] = buf[0];
    out->header[1] = buf[1];
    out->length = total_len;
    out->type = buf[3];

    uint8_t pay_len = (total_len > 0) ? total_len - 1 : 0;
    if (pay_len > FRAME_MAX_PAYLOAD_LEN) pay_len = FRAME_MAX_PAYLOAD_LEN;

    if (pay_len > 0) {
        memcpy(out->payload, &buf[4], pay_len);
    }
    out->xor_sum = buf[4 + pay_len];

    /* 验证校验 */
    if (out->xor_sum != Frame_CalcXor(&buf[3], 1 + pay_len)) return -3;

    return 0;
}

/* ──── 验证 ──── */
int Frame_Verify(const Frame_t *frame)
{
    if (!frame) return -1;
    uint8_t pay_len = (frame->length > 0) ? frame->length - 1 : 0;
    uint8_t calc = Frame_CalcXor(&frame->type, 1 + pay_len);
    return (frame->xor_sum == calc) ? 0 : -1;
}
