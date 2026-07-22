/**
 * @file    frame.h
 * @brief   通信数据帧封装与解析
 *
 * 公开接口：
 *   Frame_Pack()    - 打包帧，返回完整帧长度，0成功
 *   Frame_Unpack()  - 解析帧，提取类型和负载，返回0成功
 */

#ifndef __FRAME_H
#define __FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_HEADER1     0xAA
#define FRAME_HEADER2     0x55
#define FRAME_MAX_PAYLOAD 64

int8_t Frame_Pack(uint8_t *out_buf, uint16_t *out_len,
                  uint8_t type, uint8_t *payload, uint16_t payload_len);

int8_t Frame_Unpack(uint8_t *in_buf, uint16_t in_len,
                    uint8_t *type, uint8_t *payload, uint16_t *payload_len);

#ifdef __cplusplus
}
#endif

#endif /* __FRAME_H */