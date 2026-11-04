/******************************************************************************
* File Name: dataset_capture.c
*
* Description: OV7675 dataset capture mode, see dataset_capture.h for the frame
*              format and controls. Compiled to nothing unless
*              DATASET_CAPTURE_MODE=1 (Makefile), so the normal build, including
*              the H7 UART link, is unchanged.
*
*******************************************************************************/
#include "dataset_capture.h"

#if DATASET_CAPTURE_MODE

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cyabs_rtos.h"
#include "ifx_time_utils.h"
#include "inference_task.h"
#include "scb_uart_baud.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define CAPTURE_MAX_PIXELS          (MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT)
#define CAPTURE_HEADER_LEN          (24U)
#define CAPTURE_POLL_MS             (10U)
#define BTN_DEBOUNCE_POLLS          (3U)        /* 30 ms */
#define BTN_LONG_PRESS_MS           (1000U)
#define LED_BLINK_MS                (40U)
#define DEBUG_UART_TX_DRAIN_MS      (50U)

/*******************************************************************************
* Local Variables
*******************************************************************************/
static uint8_t capture_buf[CAPTURE_MAX_PIXELS];
static uint16_t capture_w;
static uint16_t capture_h;
static uint16_t capture_conf_milli;
static uint32_t capture_ts_ms;

/* request: written by the capture task, cleared by the inference task.
 * full:    set by the inference task after copying, cleared by the capture task. */
static volatile bool capture_request = false;
static volatile bool capture_full = false;

static uint32_t frames_sent = 0;
static cy_thread_t capture_thread;

/*******************************************************************************
* CRC-32 (IEEE 802.3, same as Python zlib.crc32)
*******************************************************************************/
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/*******************************************************************************
* Debug UART
*******************************************************************************/
static void debug_uart_set_baud(uint32_t baud)
{
    cy_stc_scb_uart_config_t cfg = CYBSP_DEBUG_UART_config;

    /* Let the boot banner leave the FIFO before changing the baud */
    for (uint32_t t = 0; (t < DEBUG_UART_TX_DRAIN_MS) && !Cy_SCB_UART_IsTxComplete(CYBSP_DEBUG_UART_HW); t++)
    {
        Cy_SysLib_Delay(1U);
    }

    Cy_SCB_UART_Disable(CYBSP_DEBUG_UART_HW, NULL);
    uint32_t actual = scb_uart_set_baud(&cfg, (en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM,
                                        CYBSP_DEBUG_UART_CLK_DIV_HW, CYBSP_DEBUG_UART_CLK_DIV_NUM, baud);
    /* Re-init keeps retarget-io working: it only uses the SCB FIFO registers */
    (void)Cy_SCB_UART_Init(CYBSP_DEBUG_UART_HW, &cfg, NULL);
    Cy_SCB_UART_Enable(CYBSP_DEBUG_UART_HW);

    printf("\r\n[DATASET] capture mode: debug UART now %lu bd (requested %lu, oversample %lu)\r\n",
           (unsigned long)actual, (unsigned long)baud, (unsigned long)cfg.oversample);
}

static void send_frame(void)
{
    uint8_t hdr[CAPTURE_HEADER_LEN];
    uint8_t tail[4];
    uint32_t len = (uint32_t)capture_w * capture_h;

    memcpy(hdr, "DSF1", 4);
    put_u32(&hdr[4], frames_sent);
    put_u32(&hdr[8], capture_ts_ms);
    put_u16(&hdr[12], capture_w);
    put_u16(&hdr[14], capture_h);
    put_u32(&hdr[16], len);
    put_u16(&hdr[20], capture_conf_milli);
    put_u16(&hdr[22], 0U);

    uint32_t crc = crc32_update(0U, &hdr[4], CAPTURE_HEADER_LEN - 4U);
    crc = crc32_update(crc, capture_buf, len);
    put_u32(tail, crc);

    /* Keep the binary frame contiguous: no other task may printf() into it.
     * At 921600 bd a 19.2 kB frame takes ~210 ms; interrupts keep running. */
    vTaskSuspendAll();
    while (!Cy_SCB_UART_IsTxComplete(CYBSP_DEBUG_UART_HW))
    {
        /* finish any text already in the FIFO */
    }
    Cy_SCB_UART_PutArrayBlocking(CYBSP_DEBUG_UART_HW, hdr, sizeof(hdr));
    Cy_SCB_UART_PutArrayBlocking(CYBSP_DEBUG_UART_HW, capture_buf, len);
    Cy_SCB_UART_PutArrayBlocking(CYBSP_DEBUG_UART_HW, tail, sizeof(tail));
    while (!Cy_SCB_UART_IsTxComplete(CYBSP_DEBUG_UART_HW))
    {
    }
    (void)xTaskResumeAll();

    frames_sent++;
}

/*******************************************************************************
* Inference task hook
*******************************************************************************/
void dataset_capture_offer(const int8_t *model_input, uint16_t width, uint16_t height,
                           float conf_human)
{
    if (!capture_request || capture_full || ((uint32_t)width * height > CAPTURE_MAX_PIXELS))
    {
        return;
    }

    /* Exactly what the model saw: q = gray - 128  ->  gray = q + 128 */
    uint32_t n = (uint32_t)width * height;
    for (uint32_t i = 0; i < n; i++)
    {
        capture_buf[i] = (uint8_t)((int32_t)model_input[i] + 128);
    }
    capture_w = width;
    capture_h = height;
    float c = conf_human * 1000.0f + 0.5f;
    capture_conf_milli = (c <= 0.0f) ? 0U : ((c >= 1000.0f) ? 1000U : (uint16_t)c);
    capture_ts_ms = (uint32_t)ifx_time_get_ms_f();

    capture_request = false;
    __DMB();
    capture_full = true;
}

/*******************************************************************************
* Capture task: SW1 + LEDs + UART streaming
*******************************************************************************/
static void led_write(GPIO_PRT_Type *port, uint32_t pin, bool on)
{
    Cy_GPIO_Write(port, pin, on ? CYBSP_LED_STATE_ON : CYBSP_LED_STATE_OFF);
}

static void dataset_capture_task(void *arg)
{
    (void)arg;
    bool continuous = (DATASET_CAPTURE_AUTOSTART != 0);
    bool btn_down = false;
    uint32_t btn_low_polls = 0;
    uint32_t btn_down_ms = 0;
    bool long_handled = false;
    uint32_t next_capture_ms = 0;
    uint32_t led_off_ms = 0;

    Cy_GPIO_Pin_Init(CYBSP_SW1_PORT, CYBSP_SW1_PIN, &CYBSP_SW1_config);
    Cy_GPIO_Pin_Init(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, &CYBSP_USER_LED1_config);
    Cy_GPIO_Pin_Init(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN, &CYBSP_USER_LED2_config);
    led_write(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN, continuous);

    debug_uart_set_baud(DATASET_CAPTURE_BAUD);
    printf("[DATASET] SW1 short = 1 frame, SW1 long (>=%u ms) = continuous every %u ms. Frames: %ux%u gray.\r\n",
           BTN_LONG_PRESS_MS, DATASET_CAPTURE_INTERVAL_MS, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT);

    for (;;)
    {
        uint32_t now = (uint32_t)ifx_time_get_ms_f();

        /* --- SW1 (active low, debounced) --- */
        bool raw_low = (Cy_GPIO_Read(CYBSP_SW1_PORT, CYBSP_SW1_PIN) == CYBSP_BTN_PRESSED);
        btn_low_polls = raw_low ? (btn_low_polls + 1U) : 0U;

        if (!btn_down && (btn_low_polls >= BTN_DEBOUNCE_POLLS))
        {
            btn_down = true;
            btn_down_ms = now;
            long_handled = false;
        }
        else if (btn_down && !long_handled && ((now - btn_down_ms) >= BTN_LONG_PRESS_MS))
        {
            long_handled = true;
            continuous = !continuous;
            next_capture_ms = now;
            led_write(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN, continuous);
            printf("[DATASET] continuous capture %s\r\n", continuous ? "ON" : "OFF");
        }
        else if (btn_down && !raw_low)
        {
            btn_down = false;
            if (!long_handled)
            {
                capture_request = true;     /* short press: one frame */
            }
        }

        /* --- continuous schedule --- */
        if (continuous && ((int32_t)(now - next_capture_ms) >= 0))
        {
            capture_request = true;
            next_capture_ms = now + DATASET_CAPTURE_INTERVAL_MS;
        }

        /* --- stream a captured frame --- */
        if (capture_full)
        {
            __DMB();
            send_frame();
            capture_full = false;
            led_write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, true);
            led_off_ms = (uint32_t)ifx_time_get_ms_f() + LED_BLINK_MS;
        }
        if ((led_off_ms != 0U) && ((int32_t)(now - led_off_ms) >= 0))
        {
            led_write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, false);
            led_off_ms = 0U;
        }

        vTaskDelay(pdMS_TO_TICKS(CAPTURE_POLL_MS));
    }
}

void dataset_capture_start(void)
{
    cy_rslt_t result = cy_rtos_thread_create(&capture_thread, &dataset_capture_task, DATASET_CAPTURE_TASK_NAME,
                                             NULL, DATASET_CAPTURE_TASK_STACK_SIZE,
                                             DATASET_CAPTURE_TASK_PRIORITY, NULL);
    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }
}

#endif /* DATASET_CAPTURE_MODE */
