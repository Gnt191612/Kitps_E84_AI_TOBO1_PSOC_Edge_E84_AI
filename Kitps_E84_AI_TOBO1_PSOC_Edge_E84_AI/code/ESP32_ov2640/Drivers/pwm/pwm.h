/**
 * @file pwm.h
 * @brief LEDC/PWM 驱动（用于云台舵机）
 *
 * 接口: PWM_Init, PWM_SetDuty
 * 使用 ESP-IDF LEDC 驱动
 */

#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 公共接口 ──── */

/**
 * @brief 初始化 PWM 通道
 * @param channel LEDC 通道号 (LEDC_CHANNEL_0 ~ LEDC_CHANNEL_7)
 * @param freq    PWM 频率 (Hz, 舵机通常 50Hz)
 * @param pin     GPIO 引脚
 * @param timer   LEDC 定时器 (LEDC_TIMER_0 ~ LEDC_TIMER_3)
 * @param speed   LEDC 速度模式 (LEDC_HIGH_SPEED_MODE / LEDC_LOW_SPEED_MODE)
 * @return 0=成功, -1=失败
 */
int PWM_Init(ledc_channel_t channel, uint32_t freq, int pin,
             ledc_timer_t timer, ledc_mode_t speed);

/**
 * @brief 设置 PWM 占空比
 * @param channel LEDC 通道号
 * @param duty    占空比 (0 ~ duty_resolution 最大值)
 *                舵机: duty=820~2460 (对应 0°~180°, freq=50Hz, res=13bit)
 * @return 0=成功, -1=失败
 */
int PWM_SetDuty(ledc_channel_t channel, uint32_t duty);

#ifdef __cplusplus
}
#endif

#endif /* PWM_H */
