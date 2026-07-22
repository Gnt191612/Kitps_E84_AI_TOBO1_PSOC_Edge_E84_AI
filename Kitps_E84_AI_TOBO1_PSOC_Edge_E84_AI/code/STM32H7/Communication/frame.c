/**
 * @file    frame.c
 * @brief   帧封装/解包实现（异或校验）
 */

#include "frame.h"
#include <string.h>

static uint8_t CalcChecksum(uint8_t *data, uint16_t len)
{
    uint8_t cs = 0;
    for (uint16_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

int8_t Frame_Pack(uint8_t *out_buf, uint16_t *out_len,
                  uint8_t type, uint8_t *payload, uint16_t payload_len)
{
    if (!out_buf || !out_len || payload_len > FRAME_MAX_PAYLOAD) return -1;

    uint16_t idx = 0;
    out_buf[idx++] = FRAME_HEADER1;
    out_buf[idx++] = FRAME_HEADER2;
    out_buf[idx++] = (uint8_t)(payload_len + 1); // 负载长度 = type + payload
    out_buf[idx++] = type;

    if (payload && payload_len > 0) {
        memcpy(&out_buf[idx], payload, payload_len);
        idx += payload_len;
    }

    /* 校验范围：type + payload（与 PSE84E frame.c 一致） */
    out_buf[idx++] = CalcChecksum(&out_buf[3], payload_len + 1);

    *out_len = idx;
    return 0;
}

int8_t Frame_Unpack(uint8_t *in_buf, uint16_t in_len,
                    uint8_t *type, uint8_t *payload, uint16_t *payload_len)
{
    if (!in_buf || in_len < 5) return -1;       // 帧头2+长度1+类型1+校验1 至少5字节
    if (in_buf[0] != FRAME_HEADER1 || in_buf[1] != FRAME_HEADER2) return -2;

    uint8_t total_len = in_buf[2];               // type + payload 的字节数
    if (total_len < 1 || in_len < (uint16_t)(4 + total_len)) return -3;  // 帧头2+长度1+后续(total+1checksum)

    uint8_t rx_type = in_buf[3];
    uint8_t *rx_payload = &in_buf[4];
    uint8_t payload_sz = total_len - 1;
    uint8_t rx_cs = in_buf[4 + payload_sz];

    if (rx_cs != CalcChecksum(&in_buf[3], total_len)) return -4;

    if (type) *type = rx_type;
    if (payload && payload_len) {
        memcpy(payload, rx_payload, payload_sz);
        *payload_len = payload_sz;
    }
    return 0;
}