/*
 * 通信测试的 84E 一侧：直接包含真实的 proj_cm55/source/h7_link.c，
 * 以便调用其中的 static 解析/应答函数；PDL/FreeRTOS 用 stubs/e84 替身。
 */
#include "h7_link.c"
#include "e84_side.h"

CySCB_Type stub_scb1;
GPIO_PRT_Type stub_prt9;
const cy_stc_scb_uart_config_t stub_uart_cfg = { 16 };
const cy_stc_gpio_pin_config_t stub_pin_cfg = { 0 };

uint8_t e84_tx[256];
int     e84_tx_len;

void Cy_SCB_UART_PutArrayBlocking(CySCB_Type *b, void *buf, uint32_t n)
{
    (void)b;
    if (e84_tx_len + (int)n <= (int)sizeof(e84_tx)) {
        memcpy(&e84_tx[e84_tx_len], buf, n);
        e84_tx_len += (int)n;
    }
}

static uint8_t s_rx[512];
static int s_rx_head, s_rx_n;
uint32_t Cy_SCB_UART_GetNumInRxFifo(CySCB_Type const *b) { (void)b; return (uint32_t)s_rx_n; }
uint32_t Cy_SCB_UART_Get(CySCB_Type const *b)
{
    (void)b;
    uint8_t v = s_rx[s_rx_head];
    s_rx_head = (s_rx_head + 1) % (int)sizeof(s_rx);
    s_rx_n--;
    return v;
}

uint32_t scb_uart_set_baud(cy_stc_scb_uart_config_t *cfg, en_clk_dst_t div_grp,
                           cy_en_divider_types_t div_type, uint32_t div_num, uint32_t baud)
{ (void)cfg; (void)div_grp; (void)div_type; (void)div_num; return baud; }
float ifx_time_get_ms_f(void) { return 0.0f; }
bool inference_get_latest_result(human_detect_result_t *out) { (void)out; return false; }
void inference_scan_track(bool enable) { (void)enable; }

void e84_feed(const uint8_t *buf, int n)
{
    for (int i = 0; i < n; i++) {
        s_rx[(s_rx_head + s_rx_n) % (int)sizeof(s_rx)] = buf[i];
        s_rx_n++;
    }
    rx_service();   /* 真实的 FIFO 读取 + 逐字节解析 */
}

int e84_pop_command(e84_cmd_t *out)
{
    h7_command_t c;
    if (!cmd_queue_pop(&c)) return 0;
    out->cmd_type = c.cmd_type; out->target_id = c.target_id;
    out->center_angle = c.center_angle; out->angle_width = c.angle_width;
    out->min_dist_cm = c.min_dist_cm; out->max_dist_cm = c.max_dist_cm;
    out->timestamp_ms = c.timestamp_ms;
    return 1;
}

uint32_t e84_rx_errors(void) { return stat_rx_errors; }

void e84_scan_frame(const e84_cmd_t *in, float conf_human, int valid, float x, float y)
{
    h7_command_t c;
    memset(&c, 0, sizeof(c));
    c.cmd_type = in->cmd_type; c.target_id = in->target_id;
    c.center_angle = in->center_angle; c.min_dist_cm = in->min_dist_cm; c.max_dist_cm = in->max_dist_cm;
    human_detect_result_t det;
    memset(&det, 0, sizeof(det));
    det.conf_human = conf_human;
    det.track_valid = valid != 0;
    det.track_x_px = x;
    det.track_y_px = y;
    e84_tx_len = 0;
    send_scan_frame(&c, &det);
}

void e84_reply(const e84_cmd_t *in, float conf_human, int timeout)
{
    h7_command_t c;
    memset(&c, 0, sizeof(c));
    c.cmd_type = in->cmd_type; c.target_id = in->target_id;
    c.center_angle = in->center_angle; c.angle_width = in->angle_width;
    c.min_dist_cm = in->min_dist_cm; c.max_dist_cm = in->max_dist_cm;
    human_detect_result_t det;
    memset(&det, 0, sizeof(det));
    det.conf_human = conf_human;
    e84_tx_len = 0;
    send_result(&c, timeout ? NULL : &det);
}
