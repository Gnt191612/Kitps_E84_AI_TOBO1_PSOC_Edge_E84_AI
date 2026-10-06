/**
 * @file    protocol.c
 * @brief   协议实现：字节→帧→回调队列
 *
 * 通信结构：
 *   84E结果帧:  [id:1B][conf:4B][x:4B][y:4B][dist:4B][angle:4B]  共21B负载（精简版）
 *               注：detected 由 confidence>0 隐式判断，timestamp 由 H7 SysTick 本地记录
 *   ESP32跟踪帧: [id:1B][x_mm:4B][y_mm:4B][pan:4B][tilt:4B][lost:1B] 共18B负载
 *
 * 依赖：
 *   - frame.h         帧打包/解包
 *   - uart.h          底层串口发送（Drivers层）
 */

#include "protocol.h"
#include "frame.h"
#include "uart.h"               // 使用 UART_Send84E / UART_SendESP32
#include "logger.h"
#include "main.h"
#include <string.h>

/* ---------- 内部常量 ---------- */
#define RX_BUF_SIZE         128
#define FRAME_QUEUE_SIZE    4

/* ---------- 回调函数指针 ---------- */
static Recv84ECallback_t       g84ECallback = NULL;
static RecvESP32Callback_t     gESP32Callback = NULL;
static RecvBrowserCmdCallback_t gBrowserCmdCallback = NULL;

/* ---------- 接收状态机 ---------- */
typedef enum {
    STATE_H1,
    STATE_H2,
    STATE_LEN,
    STATE_DATA
} RxState_t;

/* 84E 通道 */
static RxState_t g_state84E = STATE_H1;
static uint8_t   g_buf84E[RX_BUF_SIZE];
static uint16_t  g_idx84E = 0;
static uint8_t   g_payloadLen84E = 0;

/* ESP32 双通道 */
static RxState_t g_stateESP32[2] = {STATE_H1, STATE_H1};
static uint8_t   g_bufESP32[2][RX_BUF_SIZE];
static uint16_t  g_idxESP32[2] = {0, 0};
static uint8_t   g_payloadLenESP32[2] = {0, 0};

/* 完整帧队列（使用环形缓冲区，ISR安全）
 * ISR写tail, 主循环读head，天然无竞争 */
typedef struct {
    uint8_t data[FRAME_QUEUE_SIZE][FRAME_MAX_PAYLOAD + 4];
    uint8_t len[FRAME_QUEUE_SIZE];
    uint8_t source[FRAME_QUEUE_SIZE];
    volatile uint8_t head;  /* 主循环读取位置 */
    volatile uint8_t tail;  /* ISR写入位置 */
} RingFrameQueue_t;

static RingFrameQueue_t g_fq84E = {0};
static RingFrameQueue_t g_fqESP32 = {0};

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void Protocol_Init(void)
{
    g84ECallback = NULL;
    gESP32Callback = NULL;
    g_state84E = STATE_H1;
    g_stateESP32[0] = STATE_H1;
    g_stateESP32[1] = STATE_H1;
    g_fq84E.head = 0;
    g_fq84E.tail = 0;
    g_fqESP32.head = 0;
    g_fqESP32.tail = 0;
    Logger_Print(LOG_INFO, "Protocol engine ready.");
}

void Protocol_Register84ECallback(Recv84ECallback_t cb)
{
    g84ECallback = cb;
}

void Protocol_RegisterESP32Callback(RecvESP32Callback_t cb)
{
    gESP32Callback = cb;
}

void Protocol_RegisterBrowserCmdCallback(RecvBrowserCmdCallback_t cb)
{
    gBrowserCmdCallback = cb;
}

/* 将完成的帧推入环形队列（ISR中调用） */
static inline uint8_t Ring_Push(RingFrameQueue_t *q, uint8_t *buf,
                                uint8_t total_len, uint8_t source)
{
    uint8_t next_tail = (q->tail + 1) % FRAME_QUEUE_SIZE;
    if (next_tail == q->head) return 1;  /* 队列满，丢弃 */
    memcpy(q->data[q->tail], buf, total_len);
    q->len[q->tail] = total_len;
    q->source[q->tail] = source;
    q->tail = next_tail;
    return 0;
}

/* 从环形队列取出待处理帧（主循环中调用） */
static inline uint8_t Ring_Pop(RingFrameQueue_t *q, uint8_t **out_buf,
                               uint8_t *out_len, uint8_t *source)
{
    if (q->head == q->tail) return 1;  /* 队列空 */
    *out_buf = q->data[q->head];
    *out_len = q->len[q->head];
    if (source) *source = q->source[q->head];
    q->head = (q->head + 1) % FRAME_QUEUE_SIZE;
    return 0;
}

/*----------------------------------------------------------------------------
 * 字节喂入（由UART中断调用）
 *----------------------------------------------------------------------------*/
void Protocol_Feed84EByte(uint8_t byte)
{
    switch (g_state84E) {
    case STATE_H1:
        if (byte == FRAME_HEADER1) {
            g_buf84E[0] = byte;
            g_idx84E = 1;
            g_state84E = STATE_H2;
        }
        break;
    case STATE_H2:
        if (byte == FRAME_HEADER2) {
            g_buf84E[1] = byte;
            g_idx84E = 2;
            g_state84E = STATE_LEN;
        } else {
            g_state84E = STATE_H1;
        }
        break;
    case STATE_LEN:
        if (byte <= FRAME_MAX_PAYLOAD && byte > 0) {
            g_buf84E[2] = byte;
            g_payloadLen84E = byte;
            g_idx84E = 3;
            g_state84E = STATE_DATA;
        } else {
            g_state84E = STATE_H1;
        }
        break;
    case STATE_DATA:
        g_buf84E[g_idx84E++] = byte;
        if (g_idx84E >= (uint16_t)(3 + g_payloadLen84E + 1)) {
            uint16_t total = 3 + g_payloadLen84E + 1;
            uint8_t type, payload[FRAME_MAX_PAYLOAD];
            uint16_t plen;
            if (Frame_Unpack(g_buf84E, total, &type, payload, &plen) == 0) {
                Ring_Push(&g_fq84E, g_buf84E, (uint8_t)total, 0);
            }
            g_state84E = STATE_H1;
        }
        break;
    }
}

void Protocol_FeedESP32Byte(uint8_t byte, uint8_t esp_id)
{
    if (esp_id > 1) return;
    RxState_t *state = &g_stateESP32[esp_id];
    uint8_t *buf = g_bufESP32[esp_id];
    uint16_t *idx = &g_idxESP32[esp_id];
    uint8_t *payLen = &g_payloadLenESP32[esp_id];

    switch (*state) {
    case STATE_H1:
        if (byte == FRAME_HEADER1) { buf[0]=byte; *idx=1; *state=STATE_H2; }
        break;
    case STATE_H2:
        if (byte == FRAME_HEADER2) { buf[1]=byte; *idx=2; *state=STATE_LEN; }
        else *state=STATE_H1;
        break;
    case STATE_LEN:
        if (byte <= FRAME_MAX_PAYLOAD && byte > 0) {
            buf[2]=byte; *payLen=byte; *idx=3; *state=STATE_DATA;
        } else *state=STATE_H1;
        break;
    case STATE_DATA:
        buf[(*idx)++] = byte;
        if (*idx >= (3 + *payLen + 1)) {
            uint16_t total = 3 + *payLen + 1;
            uint8_t type, payload[FRAME_MAX_PAYLOAD];
            uint16_t plen;
            if (Frame_Unpack(buf, total, &type, payload, &plen) == 0) {
                Ring_Push(&g_fqESP32, buf, (uint8_t)total, esp_id);
            }
            *state = STATE_H1;
        }
        break;
    }
}

/*----------------------------------------------------------------------------
 * 发送命令（打包帧并调用底层UART发送）
 *----------------------------------------------------------------------------*/
void Protocol_Send84ECommand(uint8_t cmd_type, uint8_t target_id,
                             float angle, float width,
                             float minDist, float maxDist)
{
    uint8_t payload[22];    /* 与 PSE84E CommandPacket_t (packed=22B) 对齐 */
    payload[0] = cmd_type;
    payload[1] = target_id;
    memcpy(&payload[2], &angle, 4);
    memcpy(&payload[6], &width, 4);
    memcpy(&payload[10], &minDist, 4);
    memcpy(&payload[14], &maxDist, 4);
    uint32_t now = HAL_GetTick();
    memcpy(&payload[18], &now, 4);

    uint8_t frame[64];
    uint16_t len;
    if (Frame_Pack(frame, &len, 0x01, payload, 22) == 0) {
        UART_Send84E(frame, len);               // 改为调用Drivers/uart
    }
}

void Protocol_SendESP32Command(uint8_t esp_id, uint8_t cmd,
                               uint8_t target_id,
                               float param1, float param2)
{
    uint8_t payload[10];
    payload[0] = cmd;
    payload[1] = target_id;
    int32_t p1 = (int32_t)param1;
    int32_t p2 = (int32_t)param2;
    memcpy(&payload[2], &p1, 4);
    memcpy(&payload[6], &p2, 4);

    uint8_t frame[64];
    uint16_t len;
    if (Frame_Pack(frame, &len, 0x02, payload, 10) == 0) {
        UART_SendESP32(esp_id, frame, len);
    }
}

/*----------------------------------------------------------------------------
 * 文本上传：将文本打包为 type=0x03 帧，通过UART发到ESP32 WiFi中继
 *----------------------------------------------------------------------------*/
void Protocol_SendESP32Text(uint8_t esp_id, const char *text, uint16_t len)
{
    if (!text || len == 0) return;
    if (len > FRAME_MAX_PAYLOAD) len = FRAME_MAX_PAYLOAD;

    uint8_t frame[FRAME_MAX_PAYLOAD + 4];
    uint16_t frame_len;
    if (Frame_Pack(frame, &frame_len, FRAME_TYPE_UPLOAD, (uint8_t *)text, len) == 0) {
        UART_SendESP32(esp_id, frame, frame_len);
    }
}

/*----------------------------------------------------------------------------
 * 主循环处理：从队列取出完整帧，触发回调
 *----------------------------------------------------------------------------*/
void Protocol_ProcessIncoming(void)
{
    /* 处理84E结果帧 */
    while (1) {
        uint8_t *buf;
        uint8_t len;
        if (Ring_Pop(&g_fq84E, &buf, &len, NULL) != 0) break;

        uint8_t type;
        uint8_t payload[FRAME_MAX_PAYLOAD];
        uint16_t plen;
        if (Frame_Unpack(buf, len, &type, payload, &plen) == 0 && plen == 21) { /* ResultPacket_t = 21B */
            uint8_t id = payload[0];    /* target_id */
            float conf, x, y, dist, angle;
            memcpy(&conf, &payload[1], 4);   /* confidence */
            memcpy(&x, &payload[5], 4);     /* x_cm */
            memcpy(&y, &payload[9], 4);     /* y_cm */
            memcpy(&dist, &payload[13], 4); /* dist_cm */
            memcpy(&angle, &payload[17], 4);/* angle_deg */
            /* detected 隐式判定：confidence > 0 视为有目标；timestamp 由 H7 SysTick 本地记录 */

            if (g84ECallback) {
                g84ECallback(id, conf, x, y, dist, angle);
            }
        }
    }

    /* 处理ESP32跟踪帧 + 浏览器指令帧 */
    while (1) {
        uint8_t *buf;
        uint8_t len;
        uint8_t esp_id;
        if (Ring_Pop(&g_fqESP32, &buf, &len, &esp_id) != 0) break;

        uint8_t type;
        uint8_t payload[FRAME_MAX_PAYLOAD];
        uint16_t plen;
        if (Frame_Unpack(buf, len, &type, payload, &plen) != 0) continue;

        if (type == FRAME_TYPE_BROWSER_CMD && plen >= 2 && gBrowserCmdCallback) {
            /* 浏览器转发指令 payload=[cmd_code:1B][target_id:1B] */
            gBrowserCmdCallback(payload[0], payload[1]);
        } else if (type != FRAME_TYPE_BROWSER_CMD && plen == 18) {
            /* 跟踪结果帧 */
            uint8_t id = payload[0];
            int32_t x_raw, y_raw, pan_raw, tilt_raw;
            memcpy(&x_raw, &payload[1], 4);
            memcpy(&y_raw, &payload[5], 4);
            memcpy(&pan_raw, &payload[9], 4);
            memcpy(&tilt_raw, &payload[13], 4);
            float x = (float)x_raw;
            float y = (float)y_raw;
            float pan_ctrl = (float)pan_raw;
            float tilt_ctrl = (float)tilt_raw;
            uint8_t lost = payload[17];

            if (gESP32Callback) {
                gESP32Callback(esp_id, id, x, y,
                               pan_ctrl, tilt_ctrl, lost);
            }
        }
    }
}
