/**
 * @file gpio.c
 * @brief ESP-IDF GPIO 驱动封装实现
 */

#include "gpio.h"
#include "esp_log.h"

static const char *TAG = "GPIO";

/* ──── 初始化 ──── */
int GPIO_Init(gpio_num_t pin, GPIO_Mode_t mode, int pull_up, int pull_down)
{
    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = mode,
        .pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO_Init(%d) failed: %d", pin, ret);
    }
    return ret;
}

/* ──── 写 ──── */
void GPIO_Write(gpio_num_t pin, uint8_t val)
{
    gpio_set_level(pin, val);
}

/* ──── 读 ──── */
int GPIO_Read(gpio_num_t pin)
{
    return gpio_get_level(pin);
}

/* ──── 中断注册 ──── */
int GPIO_IsrRegister(gpio_num_t pin, GPIO_IsrHandler_t cb, void *arg, gpio_int_type_t edge)
{
    /* 确保 ISR 服务已安装 */
    static int isr_installed = 0;
    if (!isr_installed) {
        gpio_install_isr_service(0);
        isr_installed = 1;
    }

    /* 配置中断类型 */
    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT_LOCAL,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = edge,
    };
    gpio_config(&conf);

    /* 注册 ISR */
    return gpio_isr_handler_add(pin, cb, arg);
}
