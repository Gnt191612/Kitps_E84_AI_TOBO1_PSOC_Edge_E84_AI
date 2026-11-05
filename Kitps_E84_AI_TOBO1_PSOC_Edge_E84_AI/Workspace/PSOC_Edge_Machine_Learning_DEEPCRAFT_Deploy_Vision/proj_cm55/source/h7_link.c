/******************************************************************************
* File Name: h7_link.c
*
* Description: UART link to the STM32H7 master. Receives 0x01 command frames,
*              answers each with one 0x02 result frame built from the next
*              completed human/background inference (inference_task.c).
*              Ported from code/PSE84E_Project/Communication/{frame,protocol}.c.
*
* Hardware (NOT verified on the board yet):
*   SCB1 UART (BSP alias CYBSP_TENXER_UART), P9_2 = TX, P9_3 = RX on header J5
*   ("alternate serial interface"). J5 is a 1.8 V I/O domain: put a
*   1.8 V <-> 3.3 V level shifter between J5 and H7 USART1 (PB6 TX / PB7 RX).
*
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "inference_task.h"
#include "ifx_time_utils.h"
#include "h7_link.h"
#include "scb_uart_baud.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define H7_UART_HW                  CYBSP_TENXER_UART_HW            /* SCB1 */
#define H7_UART_DIV_GRP             peri_0_group_8_div_16_0_GRP_NUM /* SCB1 clock divider */
#define H7_UART_DIV_TYPE            peri_0_group_8_div_16_0_HW
#define H7_UART_DIV_NUM             peri_0_group_8_div_16_0_NUM

#define H7_RX_POLL_MS               (2U)    /* 115200 bd -> ~23 B per poll, FIFO holds more */
/* Max wait for a fresh inference after a command. H7 gives up after
 * RECOG_TIMEOUT_MS = 1500 ms (scheduler.c), so stay well below that. */
#define H7_INFER_WAIT_MS            (800U)
#define H7_CMD_QUEUE_LEN            (4U)
/* 收不到停止命令时自己结束（H7 那边一次最多 3 s） */
#define H7_SCAN_MAX_MS              (5000U)

#define DEG_TO_RAD(d)               ((d) * 0.017453292519943295f)

/*******************************************************************************
* Types
*******************************************************************************/
typedef enum {
    RX_HEADER1,
    RX_HEADER2,
    RX_LENGTH,
    RX_DATA
} rx_state_t;

/*******************************************************************************
* Local Variables
*******************************************************************************/
static cy_stc_scb_uart_context_t uart_context;

static rx_state_t rx_state = RX_HEADER1;
static uint8_t    rx_buf[FRAME_MAX_PAYLOAD + 4U];
static uint16_t   rx_idx = 0;

static h7_command_t cmd_queue[H7_CMD_QUEUE_LEN];
static uint8_t      cmd_head = 0;
static uint8_t      cmd_count = 0;

static uint32_t stat_rx_frames = 0;
static uint32_t stat_rx_errors = 0;

/*******************************************************************************
* Frame helpers
*******************************************************************************/
static uint8_t xor_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t cs = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        cs ^= data[i];
    }
    return cs;
}

int h7_frame_pack(uint8_t *out, uint16_t out_size, uint16_t *out_len, uint8_t type,
                  const uint8_t *payload, uint16_t payload_len)
{
    uint16_t total = 5U + payload_len;  /* AA 55 len type payload cs */

    if ((out == NULL) || (out_len == NULL) || (payload_len > FRAME_MAX_PAYLOAD) || (out_size < total))
    {
        return -1;
    }
    out[0] = FRAME_HEADER1;
    out[1] = FRAME_HEADER2;
    out[2] = (uint8_t)(payload_len + 1U);   /* type + payload */
    out[3] = type;
    if (payload_len > 0U)
    {
        memcpy(&out[4], payload, payload_len);
    }
    out[4U + payload_len] = xor_checksum(&out[3], (uint16_t)(payload_len + 1U));
    *out_len = total;
    return 0;
}

/* Both cores are little-endian, so memcpy of the packed fields matches the H7 */
void h7_command_decode(const uint8_t payload[H7_CMD_PAYLOAD_LEN], h7_command_t *cmd)
{
    cmd->cmd_type  = payload[0];
    cmd->target_id = payload[1];
    memcpy(&cmd->center_angle, &payload[2], 4);
    memcpy(&cmd->angle_width,  &payload[6], 4);
    memcpy(&cmd->min_dist_cm,  &payload[10], 4);
    memcpy(&cmd->max_dist_cm,  &payload[14], 4);
    memcpy(&cmd->timestamp_ms, &payload[18], 4);
}

void h7_result_encode(const h7_result_t *res, uint8_t payload[H7_RESULT_PAYLOAD_LEN])
{
    payload[0] = res->target_id;
    memcpy(&payload[1],  &res->confidence, 4);
    memcpy(&payload[5],  &res->x_cm, 4);
    memcpy(&payload[9],  &res->y_cm, 4);
    memcpy(&payload[13], &res->dist_cm, 4);
    memcpy(&payload[17], &res->angle_deg, 4);
}

#if H7_LINK_DEBUG_DUMP
static void dump_frame(const char *dir, const uint8_t *buf, uint16_t len)
{
    printf("[H7 %s]", dir);
    for (uint16_t i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\r\n");
}
#else
#define dump_frame(dir, buf, len)   do { (void)(dir); (void)(buf); (void)(len); } while (0)
#endif

/*******************************************************************************
* UART
*******************************************************************************/
static bool uart_init(void)
{
    cy_stc_scb_uart_config_t cfg = CYBSP_TENXER_UART_config;   /* 8N1, no flow control */

    /* Make sure P9_2/P9_3 are routed to SCB1 (BSP pin config) */
    Cy_GPIO_Pin_Init(CYBSP_SERIAL_INT_2_PORT, CYBSP_SERIAL_INT_2_PIN, &CYBSP_SERIAL_INT_2_config);
    Cy_GPIO_Pin_Init(CYBSP_SERIAL_INT_3_PORT, CYBSP_SERIAL_INT_3_PIN, &CYBSP_SERIAL_INT_3_config);

    /* The BSP leaves SCB1 at ~57600 bd (oversample 8) */
    uint32_t actual = scb_uart_set_baud(&cfg, (en_clk_dst_t)H7_UART_DIV_GRP, H7_UART_DIV_TYPE,
                                        H7_UART_DIV_NUM, H7_LINK_BAUD_RATE);

    if (CY_SCB_UART_SUCCESS != Cy_SCB_UART_Init(H7_UART_HW, &cfg, &uart_context))
    {
        printf("[H7] SCB1 UART init failed\r\n");
        return false;
    }
    Cy_SCB_UART_Enable(H7_UART_HW);

    printf("[H7] link on SCB1 P9_2(TX)/P9_3(RX), target %lu bd, actual ~%lu bd (oversample %lu)\r\n",
           (unsigned long)H7_LINK_BAUD_RATE, (unsigned long)actual, (unsigned long)cfg.oversample);
    if ((actual < (H7_LINK_BAUD_RATE * 97U / 100U)) || (actual > (H7_LINK_BAUD_RATE * 103U / 100U)))
    {
        printf("[H7] WARNING: baud error > 3%%, check the SCB1 clock divider\r\n");
    }
    return true;
}

static void uart_send(const uint8_t *buf, uint16_t len)
{
    Cy_SCB_UART_PutArrayBlocking(H7_UART_HW, (void *)buf, len);
}

/*******************************************************************************
* RX state machine (same framing as the legacy Protocol_ParseByte)
*******************************************************************************/
static void cmd_queue_push(const h7_command_t *cmd)
{
    if (cmd_count >= H7_CMD_QUEUE_LEN)
    {
        /* Drop the oldest: H7 normally has only one command in flight */
        cmd_head = (uint8_t)((cmd_head + 1U) % H7_CMD_QUEUE_LEN);
        cmd_count--;
    }
    cmd_queue[(cmd_head + cmd_count) % H7_CMD_QUEUE_LEN] = *cmd;
    cmd_count++;
}

static bool cmd_queue_pop(h7_command_t *cmd)
{
    if (cmd_count == 0U)
    {
        return false;
    }
    *cmd = cmd_queue[cmd_head];
    cmd_head = (uint8_t)((cmd_head + 1U) % H7_CMD_QUEUE_LEN);
    cmd_count--;
    return true;
}

static void handle_frame(const uint8_t *frame, uint16_t total_len)
{
    uint8_t len = frame[2];                 /* type + payload */
    uint8_t type = frame[3];
    uint16_t payload_len = (uint16_t)(len - 1U);

    dump_frame("RX", frame, total_len);

    if (xor_checksum(&frame[3], len) != frame[3U + len])
    {
        stat_rx_errors++;
        return;
    }
    stat_rx_frames++;

    if ((type == FRAME_TYPE_COMMAND) && (payload_len == H7_CMD_PAYLOAD_LEN))
    {
        h7_command_t cmd;
        h7_command_decode(&frame[4], &cmd);
        cmd_queue_push(&cmd);
    }
    else
    {
        /* Unknown type / size mismatch: ignore, like the legacy 84E firmware */
        stat_rx_errors++;
    }
}

static void rx_parse_byte(uint8_t byte)
{
    switch (rx_state)
    {
        case RX_HEADER1:
            if (byte == FRAME_HEADER1)
            {
                rx_buf[0] = byte;
                rx_state = RX_HEADER2;
            }
            break;

        case RX_HEADER2:
            if (byte == FRAME_HEADER2)
            {
                rx_buf[1] = byte;
                rx_state = RX_LENGTH;
            }
            else
            {
                rx_state = (byte == FRAME_HEADER1) ? RX_HEADER2 : RX_HEADER1;
            }
            break;

        case RX_LENGTH:
            if ((byte >= 1U) && (byte <= FRAME_MAX_PAYLOAD))
            {
                rx_buf[2] = byte;
                rx_idx = 3U;
                rx_state = RX_DATA;
            }
            else
            {
                rx_state = RX_HEADER1;
            }
            break;

        case RX_DATA:
        default:
            rx_buf[rx_idx++] = byte;
            /* header(2) + len(1) + type/payload(len) + checksum(1) */
            if (rx_idx >= (uint16_t)(4U + rx_buf[2]))
            {
                handle_frame(rx_buf, rx_idx);
                rx_state = RX_HEADER1;
                rx_idx = 0U;
            }
            break;
    }
}

static void rx_service(void)
{
    uint32_t status = Cy_SCB_UART_GetRxFifoStatus(H7_UART_HW);
    if (0U != (status & (CY_SCB_UART_RX_OVERFLOW | CY_SCB_UART_RX_ERR_FRAME)))
    {
        stat_rx_errors++;
        Cy_SCB_UART_ClearRxFifoStatus(H7_UART_HW, CY_SCB_UART_RX_OVERFLOW | CY_SCB_UART_RX_ERR_FRAME);
    }

    while (Cy_SCB_UART_GetNumInRxFifo(H7_UART_HW) > 0U)
    {
        rx_parse_byte((uint8_t)Cy_SCB_UART_Get(H7_UART_HW));
    }
}

/*******************************************************************************
* Result
*******************************************************************************/
static void send_result(const h7_command_t *cmd, const human_detect_result_t *det)
{
    h7_result_t res;
    uint8_t payload[H7_RESULT_PAYLOAD_LEN];
    uint8_t frame[H7_RESULT_PAYLOAD_LEN + 5U];
    uint16_t frame_len;

    res.target_id = cmd->target_id;
    /* Human probability from the classifier; 0 when no fresh inference arrived
     * in time. H7 (scheduler.c On84EResult) treats <= 0.3 as "not recognized". */
    res.confidence = (det != NULL) ? det->conf_human : 0.0f;

    /* The classifier has no localization: echo the commanded window back.
     * angle = commanded center, dist = middle of [min,max], x/y with the same
     * convention as H7 (x = d*cos(a), y = d*sin(a), a in degrees). */
    res.angle_deg = cmd->center_angle;
    res.dist_cm = 0.5f * (cmd->min_dist_cm + cmd->max_dist_cm);
    res.x_cm = res.dist_cm * cosf(DEG_TO_RAD(res.angle_deg));
    res.y_cm = res.dist_cm * sinf(DEG_TO_RAD(res.angle_deg));

    h7_result_encode(&res, payload);
    if (0 == h7_frame_pack(frame, sizeof(frame), &frame_len, FRAME_TYPE_RESULT, payload, sizeof(payload)))
    {
        uart_send(frame, frame_len);
        dump_frame("TX", frame, frame_len);
    }

    printf("$C84,frames=%lu,err=%lu\r\n", (unsigned long)stat_rx_frames, (unsigned long)stat_rx_errors);
    printf("[H7] cmd type=%u id=%u -> conf=%.3f%s\r\n", cmd->cmd_type, cmd->target_id,
           (double)res.confidence, (det != NULL) ? "" : " (inference timeout)");
}

/* 边扫描边跟踪的一帧：像素位置放在 x_cm / y_cm，看不到目标时为 -1 */
static void send_scan_frame(const h7_command_t *cmd, const human_detect_result_t *det)
{
    h7_result_t res;
    uint8_t payload[H7_RESULT_PAYLOAD_LEN];
    uint8_t frame[H7_RESULT_PAYLOAD_LEN + 5U];
    uint16_t frame_len;

    res.target_id = cmd->target_id;
    res.confidence = det->conf_human;
    res.x_cm = det->track_valid ? det->track_x_px : -1.0f;
    res.y_cm = det->track_valid ? det->track_y_px : -1.0f;
    res.angle_deg = cmd->center_angle;
    res.dist_cm = 0.5f * (cmd->min_dist_cm + cmd->max_dist_cm);

    h7_result_encode(&res, payload);
    if (0 == h7_frame_pack(frame, sizeof(frame), &frame_len, FRAME_TYPE_RESULT, payload, sizeof(payload)))
    {
        uart_send(frame, frame_len);
    }
}

/*******************************************************************************
 * Function Name: cm55_h7_link_task
 *
 * Description:
 *   Polls the SCB1 RX FIFO every H7_RX_POLL_MS, parses command frames and, for
 *   each command, waits for the first inference that completes after the command
 *   arrived, then answers with a result frame carrying the same target_id.
 *
 *******************************************************************************/
void cm55_h7_link_task(void *arg)
{
    (void)arg;
    bool busy = false;
    bool scanning = false;
    h7_command_t active_cmd;
    uint32_t baseline_frame = 0;
    float wait_start_ms = 0.0f;

    if (!uart_init())
    {
        vTaskDelete(NULL);
        return;
    }

    for (;;)
    {
        rx_service();

        if (!busy)
        {
            h7_command_t incoming;
            if (cmd_queue_pop(&incoming))
            {
                human_detect_result_t latest;
                if (incoming.cmd_type == H7_CMD_SCAN_STOP)
                {
                    if (scanning)
                    {
                        scanning = false;
                        inference_scan_track(false);
                        printf("$C84,frames=%lu,err=%lu\r\n", (unsigned long)stat_rx_frames, (unsigned long)stat_rx_errors);
                    }
                }
                else
                {
                    active_cmd = incoming;
                    baseline_frame = inference_get_latest_result(&latest) ? latest.frame_id : 0U;
                    wait_start_ms = ifx_time_get_ms_f();
                    if (incoming.cmd_type == H7_CMD_SCAN_TRACK)
                    {
                        scanning = true;
                        inference_scan_track(true);
                    }
                    else
                    {
                        if (scanning) { scanning = false; inference_scan_track(false); }
                        busy = true;
                    }
                }
            }
        }

        if (scanning)
        {
            human_detect_result_t det;
            if (inference_get_latest_result(&det) && (det.frame_id > baseline_frame))
            {
                baseline_frame = det.frame_id;
                send_scan_frame(&active_cmd, &det);
            }
            if ((ifx_time_get_ms_f() - wait_start_ms) >= (float)H7_SCAN_MAX_MS)
            {
                scanning = false;
                inference_scan_track(false);
            }
        }

        if (busy)
        {
            human_detect_result_t det;
            if (inference_get_latest_result(&det) && (det.frame_id > baseline_frame))
            {
                /* "One inference": the first result completed after the command.
                 * Its frame capture may have started up to one frame earlier. */
                send_result(&active_cmd, &det);
                busy = false;
            }
            else if ((ifx_time_get_ms_f() - wait_start_ms) >= (float)H7_INFER_WAIT_MS)
            {
                send_result(&active_cmd, NULL);
                busy = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(H7_RX_POLL_MS));
    }
}
