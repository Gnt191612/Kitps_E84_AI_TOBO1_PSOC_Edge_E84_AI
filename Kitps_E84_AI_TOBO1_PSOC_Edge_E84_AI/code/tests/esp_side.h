/* 通信测试：ESP32 一侧的适配接口（不依赖 ESP32 头文件，供测试主程序使用） */
#ifndef ESP_SIDE_H
#define ESP_SIDE_H
#include <stdint.h>

typedef struct { uint8_t cmd, id; int32_t p1, p2; } esp_cmd_t;

extern uint8_t esp_tx[256];
extern int     esp_tx_len;

void esp_init(void);
void esp_feed(const uint8_t *buf, int n);
int  esp_cmd_count(esp_cmd_t *last);
int  esp_upload_count(const uint8_t **data, int *len);
void esp_send_result(uint8_t id, float x, float y, float pan, float tilt, uint8_t lost);
void esp_comm_stats(uint32_t *frames, uint32_t *xor_errors, uint32_t *bad_len);

#endif
