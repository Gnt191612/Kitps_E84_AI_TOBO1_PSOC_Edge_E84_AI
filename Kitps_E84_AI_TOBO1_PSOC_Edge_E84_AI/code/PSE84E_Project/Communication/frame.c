/**
 * @file    frame.c
 * @brief   帧封装/解包实现（含校验和）
 */

#include "frame.h"
#include <string.h>

/* 计算校验和（异或） */
static uint8_t CalcChecksum(uint8_t *data, uint16_t len)
{
    uint8_t cs = 0;
    for (uint16_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

int8_t Frame_Pack(uint8_t *out_buf, uint16_t *out_len, uint8_t type,
                  uint8_t *payload, uint16_t payload_len)
{
    if (!out_buf || !out_len || payload_len > FRAME_MAX_PAYLOAD) {
        return -1;
    }

    uint16_t idx = 0;
    out_buf[idx++] = FRAME_HEADER1;
    out_buf[idx++] = FRAME_HEADER2;
    out_buf[idx++] = (uint8_t)(payload_len + 1); /* 总负载长度 = type(1) + payload */
    out_buf[idx++] = type;

    if (payload && payload_len > 0) {
        memcpy(&out_buf[idx], payload, payload_len);
        idx += payload_len;
    }

    /* 校验和计算范围：从 type 开始到 payload 结束 */
    uint8_t checksum = CalcChecksum(&out_buf[3], (payload_len + 1));
    out_buf[idx++] = checksum;

    *out_len = idx;
    return 0;
}

int8_t Frame_Unpack(uint8_t *in_buf, uint16_t in_len, uint8_t *type,
                    uint8_t *payload, uint16_t *payload_len)
{
    if (!in_buf || in_len < 5) {   /* 至少需要 帧头(2)+长度(1)+类型(1)+负载(?)+校验(1) */
        return -1;
    }

    /* 检查帧头 */
    if (in_buf[0] != FRAME_HEADER1 || in_buf[1] != FRAME_HEADER2) {
        return -2;
    }

    uint8_t total_size = in_buf[2];   /* type + payload 的长度 */
    if (total_size < 1 || in_len < (uint16_t)(4 + total_size)) {  /* 总帧长 = 4 + total_size */
        return -3;
    }

    uint8_t rx_type = in_buf[3];
    uint8_t *rx_payload = &in_buf[4];
    uint8_t rx_payload_len = total_size - 1;
    uint8_t rx_checksum = in_buf[4 + rx_payload_len];

    /* 验证校验和 */
    uint8_t calc_cs = CalcChecksum(&in_buf[3], total_size);
    if (calc_cs != rx_checksum) {
        return -4;
    }

    if (type) *type = rx_type;
    if (payload && payload_len) {
        if (rx_payload_len > FRAME_MAX_PAYLOAD) return -5;
        memcpy(payload, rx_payload, rx_payload_len);
        *payload_len = rx_payload_len;
    }

    return 0;
}