/**
 * @file pwm.c
 * @brief LEDC/PWM 驱动实现
 */

#include "pwm.h"
#include "esp_log.h"

static const char *TAG = "PWM";

/* 记录每个通道的分辨率位数 (默认 13bit) */
#define PWM_DEFAULT_RESOLUTION LEDC_TIMER_13_BIT
#define PWM_DEFAULT_FREQ       50   /* 舵机标准 50Hz */

/* 每个通道对应的 speed_mode (初始化时保存) */
static ledc_mode_t s_ch_speed[LEDC_CHANNEL_MAX] = {0};

/* ──── 初始化 ──── */
int PWM_Init(ledc_channel_t channel, uint32_t freq, int pin,
             ledc_timer_t timer, ledc_mode_t speed)
{
    if (freq == 0) freq = PWM_DEFAULT_FREQ;

    /* 定时器配置 */
    ledc_timer_config_t timer_conf = {
        .speed_mode      = speed,
        .timer_num       = timer,
        .duty_resolution = PWM_DEFAULT_RESOLUTION,
        .freq_hz         = freq,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %d", ret);
        return -1;
    }

    /* 通道配置 */
    ledc_channel_config_t ch_conf = {
        .channel    = channel,
        .duty       = 0,
        .gpio_num   = pin,
        .speed_mode = speed,
        .hpoint     = 0,
        .timer_sel  = timer,
    };
    ret = ledc_channel_config(&ch_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %d", ret);
        return -1;
    }

    /* 保存 speed_mode 供 PWM_SetDuty 使用 */
    if (channel < LEDC_CHANNEL_MAX) {
        s_ch_speed[channel] = speed;
    }

    ESP_LOGI(TAG, "PWM init: ch=%d, freq=%" PRIu32 "Hz, pin=%d", channel, freq, pin);
    return 0;
}

/* ──── 设置占空比 ──── */
int PWM_SetDuty(ledc_channel_t channel, uint32_t duty)
{
    ledc_mode_t speed = LEDC_LOW_SPEED_MODE;  /* S3 只有低速模式 */
    if (channel < LEDC_CHANNEL_MAX && s_ch_speed[channel] != 0) {
        speed = s_ch_speed[channel];
    }

    esp_err_t ret = ledc_set_duty(speed, channel, duty);
    if (ret != ESP_OK) return -1;
    ret = ledc_update_duty(speed, channel);
    return (ret == ESP_OK) ? 0 : -1;
}
