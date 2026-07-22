/**
 * @file frame.h
 * @brief 通信帧打包/解包 (AA 55 协议, 与 H7 端完全兼容)
 *
 * 帧格式: AA 55 [负载总长=type(1)+payload] [type] [payload...] [xor校验]
 * 校验: 从 length 字段开始到 payload 末尾异或
 */

 #ifndef FRAME_H
 #define FRAME_H
 
 #include <stdint.h>
 #include <stddef.h>
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /* ──────────────────── 帧常量 ──────────────────── */
 #define FRAME_HEADER1           0xAA
 #define FRAME_HEADER2           0x55
 
 #define FRAME_TYPE_CMD          0x02   /* H7 → ESP32 指令 */
 #define FRAME_TYPE_RESULT       0x02   /* ESP32 → H7 结果  (共用 type=0x02) */
 #define FRAME_TYPE_BROWSER_CMD  0x04   /* ESP32 → H7 浏览器转发指令 */
 
 #define FRAME_HEADER_SIZE       2      /* AA 55 */
 #define FRAME_LENGTH_SIZE       1      /* 负载总长 */
 #define FRAME_TYPE_SIZE         1      /* type */
 #define FRAME_XOR_SIZE          1      /* xor校验 */
 #define FRAME_OVERHEAD          (FRAME_HEADER_SIZE + FRAME_LENGTH_SIZE + FRAME_TYPE_SIZE + FRAME_XOR_SIZE)
 
 /* 最大帧负载 (payload + type) */
 #define FRAME_MAX_PAYLOAD_LEN   64
 
 /* ──────────────────── 帧结构 ──────────────────── */
 typedef struct {
     uint8_t header[2];       /* AA 55 */
     uint8_t length;          /* 负载总长 = type(1) + payload */
     uint8_t type;            /* 帧类型 */
     uint8_t payload[FRAME_MAX_PAYLOAD_LEN];
     uint8_t xor_sum;         /* 校验和 */
 } Frame_t;
 
 /* ──────────────────── 公共接口 ──────────────────── */
 
 /**
  * @brief 计算 XOR 校验和
  * @param data  数据起始指针（从 length 字段开始）
  * @param len   数据长度（包括 type 和 payload）
  * @return XOR 校验和
  */
 uint8_t Frame_CalcXor(const uint8_t *data, uint8_t len);
 
 /**
  * @brief 打包一帧数据
  * @param type    帧类型
  * @param payload 负载数据
  * @param pay_len 负载长度
  * @param buf     输出缓冲区（至少 FRAME_OVERHEAD + pay_len 字节）
  * @return 帧总长度 (bytes)
  */
 uint16_t Frame_Pack(uint8_t type, const uint8_t *payload, uint8_t pay_len, uint8_t *buf);
 
 /**
  * @brief 解包一帧数据
  * @param buf  原始接收数据
  * @param len  buf 长度
  * @param out  输出帧结构
  * @return 0=成功, -1=头部错误, -2=长度错误, -3=校验错误
  */
 int Frame_Unpack(const uint8_t *buf, uint16_t len, Frame_t *out);
 
 /**
  * @brief 验证帧校验
  * @param frame 帧指针
  * @return 0=校验正确, -1=校验错误
  */
 int Frame_Verify(const Frame_t *frame);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* FRAME_H */
