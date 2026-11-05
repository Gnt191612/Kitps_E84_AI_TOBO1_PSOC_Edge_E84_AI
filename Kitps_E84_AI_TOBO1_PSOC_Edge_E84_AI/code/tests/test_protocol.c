/*
 * 三板通信互通测试：H7 真实 frame.c/protocol.c ↔ ESP32 真实 frame.c/protocol.c
 * ↔ 84E 真实 h7_link.c。字节流在内存中直接对接，验证双方解出的字段一致。
 */
#include "test.h"
#include "main.h"
#include "protocol.h"
#include "logger.h"
#include "esp_side.h"
#include "e84_side.h"
#include <string.h>
#include <stdlib.h>

TEST_MAIN_DEFS

/* 逐位比较两个 float（32 位 MinGW 在 C11 下按 long double 求值浮点常量，直接 == 不可靠） */
static int feq(float a, float b) { return memcmp(&a, &b, sizeof(float)) == 0; }

/* ---- H7 一侧替身 ---- */
TIM_TypeDef stub_TIM2, stub_TIM3;
GPIO_TypeDef stub_GPIOA, stub_GPIOB, stub_GPIOC, stub_GPIOD;
TIM_HandleTypeDef htim2, htim3, htim6;
UART_HandleTypeDef huart1, huart2, huart3, huart4;
uint32_t stub_tick = 12345;
uint32_t stub_capture_value;
void stub_set_compare(TIM_HandleTypeDef *h, uint32_t ch, uint32_t v) { (void)h; (void)ch; (void)v; }
void Error_Handler(void) { abort(); }
void Logger_Print(LogLevel_t level, const char *format, ...) { (void)level; (void)format; }

static uint8_t h7_to_84e[256];  static int h7_to_84e_len;
static uint8_t h7_to_esp[256];  static int h7_to_esp_len;
void UART_Send84E(uint8_t *data, uint16_t len)
{ memcpy(&h7_to_84e[h7_to_84e_len], data, len); h7_to_84e_len += len; }
void UART_SendESP32(uint8_t esp_id, uint8_t *data, uint16_t len)
{ (void)esp_id; memcpy(&h7_to_esp[h7_to_esp_len], data, len); h7_to_esp_len += len; }

/* H7 回调记录 */
static struct { int n; uint8_t id; float conf, x, y, dist, angle; } r84;
static struct { int n; uint8_t esp, id, lost; float x, y, pan, tilt; } resp;
static void on84(uint8_t id, float conf, float x, float y, float dist, float angle)
{ r84.n++; r84.id = id; r84.conf = conf; r84.x = x; r84.y = y; r84.dist = dist; r84.angle = angle; }
static void onesp(uint8_t esp, uint8_t id, float x, float y, float pan, float tilt, uint8_t lost)
{ resp.n++; resp.esp = esp; resp.id = id; resp.x = x; resp.y = y; resp.pan = pan; resp.tilt = tilt; resp.lost = lost; }

static void h7_reset(void)
{
    Protocol_Init();
    Protocol_Register84ECallback(on84);
    Protocol_RegisterESP32Callback(onesp);
    memset(&r84, 0, sizeof(r84)); memset(&resp, 0, sizeof(resp));
    h7_to_84e_len = h7_to_esp_len = 0;
}

static void test_h7_to_esp32(void)
{
    printf("[H7 -> ESP32]\n");
    h7_reset(); esp_init();
    Protocol_SendESP32Command(1, 0x10, 2, -7.6f, 231.0f);
    esp_feed(h7_to_esp, h7_to_esp_len);
    esp_cmd_t c;
    CHECK(esp_cmd_count(&c) == 1, "ESP32 parsed one command");
    CHECK(c.cmd == 0x10 && c.id == 2, "cmd/id: 0x%02X/%u", c.cmd, c.id);
    CHECK(c.p1 == -7 && c.p2 == 231, "params: %ld/%ld (int32, truncated toward zero)", (long)c.p1, (long)c.p2);

    /* 前面夹杂乱码和一个校验错误的帧：错误帧丢弃，后面的正确帧照收 */
    uint8_t junk[] = { 0x12, 0xAA, 0x00, 0x55, 0xAA, 0x55, 0x0B, 0x02, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xFF };
    esp_init();
    h7_to_esp_len = 0;
    esp_feed(junk, sizeof(junk));
    Protocol_SendESP32Command(0, 0x13, 0, 0, 0);
    esp_feed(h7_to_esp, h7_to_esp_len);
    CHECK(esp_cmd_count(&c) == 1 && c.cmd == 0x13, "only the valid RELEASE frame is delivered (n=%d)", esp_cmd_count(NULL));

    /* 文本上传：超长文本截断到 63 字节，接收方能收下 */
    char text[100];
    memset(text, 'A', sizeof(text));
    esp_init();
    h7_to_esp_len = 0;
    Protocol_SendESP32Text(0, text, sizeof(text));
    esp_feed(h7_to_esp, h7_to_esp_len);
    int len = 0;
    CHECK(esp_upload_count(NULL, &len) == 1, "long text frame accepted by ESP32");
    CHECK(len == 63, "text truncated to 63 bytes, got %d", len);
}

static void test_esp32_to_h7(void)
{
    printf("[ESP32 -> H7]\n");
    h7_reset(); esp_init();
    esp_send_result(3, 184.3f, 112.5f, 37.9f, -5.2f, 0);
    for (int i = 0; i < esp_tx_len; i++) Protocol_FeedESP32Byte(esp_tx[i], 1);
    Protocol_ProcessIncoming();
    CHECK(resp.n == 1, "H7 received one ESP32 result");
    CHECK(resp.esp == 1 && resp.id == 3 && resp.lost == 0, "esp/id/lost %u/%u/%u", resp.esp, resp.id, resp.lost);
    CHECK_NEAR(resp.x, 184, 0.01, "x (int32 pseudo-mm)");
    CHECK_NEAR(resp.pan, 37, 0.01, "pan ctrl");
    CHECK_NEAR(resp.tilt, -5, 0.01, "tilt ctrl");

    /* 两块板交错发送：按通道分别组帧，互不干扰 */
    h7_reset();
    esp_send_result(1, 100, 100, 1, 1, 0);
    uint8_t a[64]; int al = esp_tx_len; memcpy(a, esp_tx, al);
    esp_send_result(2, 200, 50, 2, 2, 1);
    for (int i = 0; i < al || i < esp_tx_len; i++) {
        if (i < al) Protocol_FeedESP32Byte(a[i], 0);
        if (i < esp_tx_len) Protocol_FeedESP32Byte(esp_tx[i], 1);
    }
    Protocol_ProcessIncoming();
    CHECK(resp.n == 2, "two interleaved frames from two boards both delivered (n=%d)", resp.n);
}

static void test_h7_to_84e(void)
{
    printf("[H7 <-> 84E]\n");
    h7_reset();
    Protocol_Send84ECommand(0, 2, 33.5f, 20.0f, 150.0f, 250.0f);
    e84_feed(h7_to_84e, h7_to_84e_len);
    e84_cmd_t c;
    CHECK(e84_pop_command(&c), "84E parsed the command");
    CHECK(c.cmd_type == 0 && c.target_id == 2, "type/id %u/%u", c.cmd_type, c.target_id);
    CHECK(feq(c.center_angle, 33.5f) && feq(c.angle_width, 20.0f), "angles exact");
    CHECK(feq(c.min_dist_cm, 150.0f) && feq(c.max_dist_cm, 250.0f), "distances exact");
    CHECK(c.timestamp_ms == stub_tick, "timestamp %lu", (unsigned long)c.timestamp_ms);
    CHECK(e84_rx_errors() == 0, "no 84E rx errors");

    e84_reply(&c, 0.873f, 0);
    for (int i = 0; i < e84_tx_len; i++) Protocol_Feed84EByte(e84_tx[i]);
    Protocol_ProcessIncoming();
    CHECK(r84.n == 1, "H7 received the 84E result");
    CHECK(r84.id == 2 && feq(r84.conf, 0.873f), "id/conf %u/%.3f", r84.id, (double)r84.conf);
    CHECK_NEAR(r84.angle, 33.5, 1e-4, "echoed angle");
    CHECK_NEAR(r84.dist, 200.0, 1e-3, "echoed window centre distance");

    /* 推理超时：置信度 0，H7 会当作非人体处理 */
    e84_reply(&c, 0.9f, 1);
    for (int i = 0; i < e84_tx_len; i++) Protocol_Feed84EByte(e84_tx[i]);
    Protocol_ProcessIncoming();
    CHECK(r84.n == 2 && feq(r84.conf, 0.0f), "timeout reply has confidence 0");

    /* 命令帧某字节损坏：84E 丢弃并计错误 */
    h7_reset();
    Protocol_Send84ECommand(0, 3, 10, 20, 100, 200);
    h7_to_84e[8] ^= 0x40;
    e84_feed(h7_to_84e, h7_to_84e_len);
    CHECK(!e84_pop_command(&c), "corrupted command rejected");
    CHECK(e84_rx_errors() >= 1, "rx error counted");
}

/* 接收统计：正确帧、校验错误、长度错误、队列溢出、串口错误都要计入对应通道 */
static void test_comm_stats(void)
{
    printf("[comm stats]\n");
    Protocol_CommStats_t s;
    h7_reset(); esp_init();
    esp_send_result(1, 100, 100, 1, 1, 0);
    uint8_t good[64]; int n = esp_tx_len; memcpy(good, esp_tx, n);

    for (int i = 0; i < n; i++) Protocol_FeedESP32Byte(good[i], 1);
    Protocol_GetCommStats(PROTO_CH_ESP32_B, &s);
    CHECK(s.frames == 1 && s.xor_errors == 0, "good frame counted on channel B");
    Protocol_GetCommStats(PROTO_CH_ESP32_A, &s);
    CHECK(s.frames == 0, "channel A untouched");

    uint8_t bad[64]; memcpy(bad, good, n); bad[n - 1] ^= 0x01;
    for (int i = 0; i < n; i++) Protocol_FeedESP32Byte(bad[i], 0);
    uint8_t badlen[] = { 0xAA, 0x55, 0x00 };
    for (int i = 0; i < 3; i++) Protocol_FeedESP32Byte(badlen[i], 0);
    Protocol_NoteUartError(PROTO_CH_ESP32_A);
    Protocol_GetCommStats(PROTO_CH_ESP32_A, &s);
    CHECK(s.xor_errors == 1, "xor error counted, got %lu", (unsigned long)s.xor_errors);
    CHECK(s.bad_len == 1, "bad length counted, got %lu", (unsigned long)s.bad_len);
    CHECK(s.uart_errors == 1, "uart error counted");

    /* 不取走队列：环形队列最多存 15 帧，之后计为丢弃 */
    h7_reset();
    for (int k = 0; k < 20; k++)
        for (int i = 0; i < n; i++) Protocol_FeedESP32Byte(good[i], 0);
    Protocol_GetCommStats(PROTO_CH_ESP32_A, &s);
    CHECK(s.frames == 15 && s.queue_drops == 5, "frames/drops %lu/%lu",
          (unsigned long)s.frames, (unsigned long)s.queue_drops);

    /* ESP32 一侧 */
    uint32_t f, x, l;
    esp_init();
    h7_to_esp_len = 0;
    Protocol_SendESP32Command(0, 0x13, 0, 0, 0);
    esp_feed(h7_to_esp, h7_to_esp_len);
    h7_to_esp[h7_to_esp_len - 1] ^= 0x01;
    esp_feed(h7_to_esp, h7_to_esp_len);
    uint8_t esp_badlen[] = { 0xAA, 0x55, 0x00 };
    esp_feed(esp_badlen, 3);
    esp_comm_stats(&f, &x, &l);
    CHECK(f == 1 && x == 1 && l == 1, "ESP32 frames/xor/bad_len %lu/%lu/%lu",
          (unsigned long)f, (unsigned long)x, (unsigned long)l);
}

int main(void)
{
    test_h7_to_esp32();
    test_esp32_to_h7();
    test_h7_to_84e();
    test_comm_stats();
    TEST_REPORT("protocol");
}
