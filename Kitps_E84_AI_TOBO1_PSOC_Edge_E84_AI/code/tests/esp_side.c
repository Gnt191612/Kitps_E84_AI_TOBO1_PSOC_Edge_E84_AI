/*
 * 通信测试的 ESP32 一侧：链接真实的 ESP32 Communication/frame.c 与 protocol.c。
 * 与 H7 同名的函数（Protocol_Init、Frame_Pack 等）在编译时用 -D 改名，避免冲突，
 * 见 run_tests.ps1 中的 ESP_RENAME。
 */
#include "Communication/protocol.h"
#include "Drivers/uart/uart.h"
#include "esp_side.h"
#include <string.h>

uint8_t esp_tx[256];
int     esp_tx_len;

int UART_Send(uart_port_t uart_num, const uint8_t *data, uint16_t len)
{
    (void)uart_num;
    if (esp_tx_len + len <= (int)sizeof(esp_tx)) {
        memcpy(&esp_tx[esp_tx_len], data, len);
        esp_tx_len += len;
    }
    return len;
}

static esp_cmd_t     s_last_cmd;
static int           s_cmd_n;
static uint8_t       s_upload[256];
static int           s_upload_len, s_upload_n;

static void on_cmd(uint8_t cmd, uint8_t target_id, int32_t p1, int32_t p2)
{
    s_last_cmd.cmd = cmd; s_last_cmd.id = target_id; s_last_cmd.p1 = p1; s_last_cmd.p2 = p2;
    s_cmd_n++;
}

static void on_upload(const uint8_t *data, uint16_t len)
{
    memcpy(s_upload, data, len);
    s_upload_len = len;
    s_upload_n++;
}

void esp_init(void)
{
    Protocol_Init(NULL, on_cmd);
    Protocol_RegisterUploadCallback(on_upload);
    s_cmd_n = 0; s_upload_n = 0; esp_tx_len = 0;
}

void esp_feed(const uint8_t *buf, int n)
{
    for (int i = 0; i < n; i++) Protocol_ParseByte(Protocol_GetHandle(), buf[i]);
}

int esp_cmd_count(esp_cmd_t *last) { if (last) *last = s_last_cmd; return s_cmd_n; }
int esp_upload_count(const uint8_t **data, int *len)
{
    if (data) *data = s_upload;
    if (len) *len = s_upload_len;
    return s_upload_n;
}

void esp_send_result(uint8_t id, float x, float y, float pan, float tilt, uint8_t lost)
{
    esp_tx_len = 0;
    Protocol_SendTrackResult(id, x, y, pan, tilt, lost);
}

void esp_comm_stats(uint32_t *frames, uint32_t *xor_errors, uint32_t *bad_len)
{
    Proto_CommStats_t s;
    Protocol_GetCommStats(&s);
    *frames = s.frames;
    *xor_errors = s.xor_errors;
    *bad_len = s.bad_len;
}
