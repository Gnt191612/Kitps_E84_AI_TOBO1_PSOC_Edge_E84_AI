/**
 * @file uart.c
 * @brief ESP-IDF UART 驱动封装实现
 */

#include "uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UART";

/* ──── 每端口回调表 ──── */
#define UART_PORT_MAX 3
static UART_RxCallback_t s_rx_cb[UART_PORT_MAX] = { NULL, NULL, NULL };
static QueueHandle_t s_uart_queue[UART_PORT_MAX] = { NULL, NULL, NULL };

/* ──── 初始化 ──── */
int UART_Init(uart_port_t uart_num, uint32_t baud, int tx_pin, int rx_pin)
{
    if (uart_num < 0 || uart_num >= UART_PORT_MAX) return -1;

    /* UART 配置 */
    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(uart_num, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config(%d) failed: %d", uart_num, ret);
        return -1;
    }

    /* 引脚 */
    ret = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin(%d) failed: %d", uart_num, ret);
        return -1;
    }

    /* 安装驱动: buffer size 256 bytes */
    const int rx_buffer_size = 256;
    const int tx_buffer_size = 256;
    ret = uart_driver_install(uart_num, rx_buffer_size, tx_buffer_size, 20, &s_uart_queue[uart_num], 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install(%d) failed: %d", uart_num, ret);
        return -1;
    }

    ESP_LOGI(TAG, "UART%d init OK, baud=%" PRIu32 ", tx=%d, rx=%d", uart_num, baud, tx_pin, rx_pin);
    return 0;
}

/* ──── 发送 ──── */
int UART_Send(uart_port_t uart_num, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return 0;
    int written = uart_write_bytes(uart_num, (const char *)data, len);
    return written;
}

/* ──── 设置接收回调 ──── */
void UART_SetRxCallback(uart_port_t uart_num, UART_RxCallback_t cb)
{
    if (uart_num >= 0 && uart_num < UART_PORT_MAX) {
        s_rx_cb[uart_num] = cb;
    }
}

/* ──── UART 接收任务 ──── */
void UART_RxTask(void *arg)
{
    uart_port_t uart_num = (uart_port_t)(intptr_t)arg;
    uint8_t rx_byte;

    ESP_LOGI(TAG, "UART_RxTask started for UART%d", uart_num);

    while (1) {
        /* 从队列读取事件 */
        uart_event_t event;
        if (xQueueReceive(s_uart_queue[uart_num], &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (event.type) {
            case UART_DATA:
                /* 读取FIFO中所有可用字节（UART_DATA事件可能对应多个字节） */
                while (uart_read_bytes(uart_num, &rx_byte, 1, 0) > 0) {
                    if (s_rx_cb[uart_num]) {
                        s_rx_cb[uart_num](rx_byte);
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART%d FIFO overflow", uart_num);
                uart_flush_input(uart_num);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART%d buffer full", uart_num);
                uart_flush_input(uart_num);
                break;

            default:
                break;
            }
        }
        /* 不再需要兜底读取：事件驱动已覆盖 */
    }
}
