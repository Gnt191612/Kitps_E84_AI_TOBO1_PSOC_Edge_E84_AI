/**
 * @file    radar_driver.c
 * @brief   HC-SR04超声波硬件驱动实现（非阻塞，TIM3_CH1 输入捕获）
 */

#include "radar_driver.h"
#include <string.h>

/*----------------------------------------------------------------------------
 * 内部状态
 *----------------------------------------------------------------------------*/
static RadarSensor_t g_sensor = {0};
static float g_sound_speed_cm_us = RADAR_SOUND_SPEED_CM_US;
static uint8_t g_initialized = 0U;

/*----------------------------------------------------------------------------
 * TRIG 高电平延时：≥10μs（400MHz 下约 15μs）
 *----------------------------------------------------------------------------*/
static void Radar_Delay_15us(void)
{
    volatile uint32_t cnt = 2400;
    while (cnt--) {
        __NOP();
    }
}

/*----------------------------------------------------------------------------
 * 初始化：只开 CH1 捕获中断。TIM3 计数器由云台 PWM 共用，此处不复位、
 * 不开更新中断。
 *----------------------------------------------------------------------------*/
void Radar_Driver_Init(void)
{
    memset(&g_sensor, 0, sizeof(g_sensor));
    g_sensor.capture_state = RADAR_CAPTURE_IDLE;

    __HAL_TIM_SET_CAPTUREPOLARITY(RADAR_TIM_HANDLE, RADAR_TIM_CHANNEL,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
    HAL_TIM_IC_Start_IT(RADAR_TIM_HANDLE, RADAR_TIM_CHANNEL);

    HAL_GPIO_WritePin(RADAR_TRIG_PORT, RADAR_TRIG_PIN, GPIO_PIN_RESET);
    g_initialized = 1U;
}

/*----------------------------------------------------------------------------
 * 触发一次测量（立即返回）
 *----------------------------------------------------------------------------*/
RadarStatus_t Radar_Trigger(void)
{
    if (!g_initialized) return RADAR_NO_INIT;
    if (g_sensor.capture_state != RADAR_CAPTURE_IDLE) return RADAR_BUSY;
    if ((HAL_GetTick() - g_sensor.last_trigger_tick) < RADAR_MIN_INTERVAL_MS) {
        return RADAR_BUSY;
    }

    __HAL_TIM_SET_CAPTUREPOLARITY(RADAR_TIM_HANDLE, RADAR_TIM_CHANNEL,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
    g_sensor.capture_state = RADAR_CAPTURE_WAIT_RISE;

    /* 发送 ≥10μs TRIG 高电平 (PC0) */
    HAL_GPIO_WritePin(RADAR_TRIG_PORT, RADAR_TRIG_PIN, GPIO_PIN_SET);
    Radar_Delay_15us();
    HAL_GPIO_WritePin(RADAR_TRIG_PORT, RADAR_TRIG_PIN, GPIO_PIN_RESET);

    g_sensor.last_trigger_tick = HAL_GetTick();
    return RADAR_OK;
}

/*----------------------------------------------------------------------------
 * 取结果（非阻塞）
 *----------------------------------------------------------------------------*/
RadarResult_t Radar_Poll(float *dist_cm)
{
    RadarCaptureState_t st = g_sensor.capture_state;

    if (st == RADAR_CAPTURE_COMPLETE) {
        float d = Radar_UltrasonicToDistance(g_sensor.pulse_width_us);
        g_sensor.capture_state = RADAR_CAPTURE_IDLE;
        if (d < RADAR_MIN_DISTANCE_CM || d > RADAR_MAX_DISTANCE_CM) {
            return RADAR_RESULT_NO_ECHO;   /* 超量程不钳位，视为无物体 */
        }
        if (dist_cm) *dist_cm = d;
        return RADAR_RESULT_OK;
    }

    if ((st == RADAR_CAPTURE_WAIT_RISE || st == RADAR_CAPTURE_GOT_RISE) &&
        (HAL_GetTick() - g_sensor.last_trigger_tick) > RADAR_ECHO_TIMEOUT_MS) {
        g_sensor.capture_state = RADAR_CAPTURE_IDLE;
        __HAL_TIM_SET_CAPTUREPOLARITY(RADAR_TIM_HANDLE, RADAR_TIM_CHANNEL,
                                      TIM_INPUTCHANNELPOLARITY_RISING);
        return RADAR_RESULT_NO_ECHO;
    }

    return RADAR_RESULT_NONE;
}

/*----------------------------------------------------------------------------
 * 脉宽 → 距离（不钳位，由调用方判断量程）
 *----------------------------------------------------------------------------*/
float Radar_UltrasonicToDistance(uint32_t pulse_width_us)
{
    if (pulse_width_us == 0) return 0.0f;
    return (float)pulse_width_us * g_sound_speed_cm_us * 0.5f;
}

uint8_t Radar_Is_Busy(void)
{
    return (g_sensor.capture_state != RADAR_CAPTURE_IDLE ||
            (HAL_GetTick() - g_sensor.last_trigger_tick) < RADAR_MIN_INTERVAL_MS) ? 1 : 0;
}

/*----------------------------------------------------------------------------
 * 温度补偿
 *----------------------------------------------------------------------------*/
void Radar_Set_Temperature(float temp_celsius)
{
    float speed_m_s   = 331.4f + 0.607f * temp_celsius;
    g_sound_speed_cm_us = speed_m_s * 0.0001f;
}

/*----------------------------------------------------------------------------
 * 输入捕获中断回调
 *
 * 脉宽可能跨越多个 TIM3 周期（20ms，4m 回波约 23ms）。先取捕获值之差
 * 模周期得到不足一个周期的部分，再用毫秒时基估计整周期数：
 *   k = round((elapsed_ms*1000 - frac) / 20000)
 * 毫秒时基误差 ±1ms 远小于 20ms 周期，k 不会判错。
 *----------------------------------------------------------------------------*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != RADAR_TIM_HANDLE->Instance) return;
    if (htim->Channel != RADAR_TIM_ACTIVE_CH) return;

    uint32_t cap = HAL_TIM_ReadCapturedValue(htim, RADAR_TIM_CHANNEL);

    if (g_sensor.capture_state == RADAR_CAPTURE_WAIT_RISE) {
        g_sensor.rise_cap = cap;
        g_sensor.rise_ms = HAL_GetTick();
        g_sensor.capture_state = RADAR_CAPTURE_GOT_RISE;
        __HAL_TIM_SET_CAPTUREPOLARITY(htim, RADAR_TIM_CHANNEL,
                                      TIM_INPUTCHANNELPOLARITY_FALLING);

    } else if (g_sensor.capture_state == RADAR_CAPTURE_GOT_RISE) {
        uint32_t frac = (cap + RADAR_TIM_PERIOD_US - g_sensor.rise_cap) % RADAR_TIM_PERIOD_US;
        int32_t elapsed_us = (int32_t)(HAL_GetTick() - g_sensor.rise_ms) * 1000;
        int32_t k = (elapsed_us - (int32_t)frac + (int32_t)(RADAR_TIM_PERIOD_US / 2)) /
                    (int32_t)RADAR_TIM_PERIOD_US;
        if (k < 0) k = 0;
        g_sensor.pulse_width_us = frac + (uint32_t)k * RADAR_TIM_PERIOD_US;
        g_sensor.capture_state = RADAR_CAPTURE_COMPLETE;
        __HAL_TIM_SET_CAPTUREPOLARITY(htim, RADAR_TIM_CHANNEL,
                                      TIM_INPUTCHANNELPOLARITY_RISING);
    }
    /* IDLE/COMPLETE 状态下的边沿（串扰、上次残留）忽略 */
}
