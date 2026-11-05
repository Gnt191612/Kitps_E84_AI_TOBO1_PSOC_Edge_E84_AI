/* 通信测试：84E 一侧的适配接口 */
#ifndef E84_SIDE_H
#define E84_SIDE_H
#include <stdint.h>

typedef struct {
    uint8_t cmd_type, target_id;
    float center_angle, angle_width, min_dist_cm, max_dist_cm;
    uint32_t timestamp_ms;
} e84_cmd_t;

extern uint8_t e84_tx[256];
extern int     e84_tx_len;

void     e84_feed(const uint8_t *buf, int n);
int      e84_pop_command(e84_cmd_t *out);
uint32_t e84_rx_errors(void);
/* 按命令应答；timeout=1 模拟推理超时（置信度 0） */
void     e84_reply(const e84_cmd_t *in, float conf_human, int timeout);
#endif
