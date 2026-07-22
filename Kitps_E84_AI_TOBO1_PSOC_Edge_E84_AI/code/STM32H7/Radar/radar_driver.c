/**
 * @file    radar_driver.c
 * @brief   HC-SR04超声波驱动核心实现
 *          基于STM32H743ZIT6 TIM3_CH1 (PC6) 输入捕获
 * 
 * 时序说明：
 *   1. 给TRIG (PC0) ≥10μs 高电平，模块自动发送8个40kHz方波
 *   2. ECHO (PC6) 输出与测量距离成正比的高电平
 *   3. 通过 TIM3_CH1 输入捕获测量高电平持续时间
 *   4. 距离(cm) = 高电平时间(μs) × 声速(cm/μs) / 2
 */


#include "radar_driver.h"
#include <string.h>

/*----------------------------------------------------------------------------
 * 全局变量
 *----------------------------------------------------------------------------*/
static RadarSensor_t g_sensor = {0};
static float g_sound_speed_cm_us = RADAR_SOUND_SPEED_CM_US;

/*----------------------------------------------------------------------------
 * 硬件抽象层(轻量级延时函数, 约15μs)
 * STM32H743@400MHz: 15μs ≈ 6000个指令周期
 *----------------------------------------------------------------------------*/
static void Radar_Delay_15us(void)
{
    /* 
     * STM32H743主频400MHz，15μs ≈ 6000个指令周期。
     * 简单软件延时即可满足TRIG信号的10μs时序要求。
     * 如需更高精度，可改用DWT->CYCCNT实现。
     */
    volatile uint32_t cnt = 2400;
    while (cnt--) {
        __NOP();
    }
}

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void Radar_Driver_Init(void)
{
    HAL_TIM_Base_Start_IT(RADAR_TIM_HANDLE);
    HAL_TIM_IC_Start_IT(RADAR_TIM_HANDLE, RADAR_TIM_CHANNEL);

    memset(&g_sensor, 0, sizeof(g_sensor));
    g_sensor.sensor_id = 0;

    HAL_GPIO_WritePin(RADAR_TRIG_PORT, RADAR_TRIG_PIN, GPIO_PIN_RESET);
}


/*----------------------------------------------------------------------------
 * 触发测量
 *----------------------------------------------------------------------------*/
RadarStatus_t Radar_Trigger(void)
{
    RadarSensor_t *s = &g_sensor;

    if ((HAL_GetTick() - s->last_trigger_tick) < RADAR_MIN_INTERVAL_MS) {
        return RADAR_BUSY;
    }

    s->data_ready     = 0;
    s->timeout_flag   = 0;
    s->capture_state  = RADAR_CAPTURE_WAIT_RISE;
    s->overflow_count = 0;
    __HAL_TIM_SET_COUNTER(RADAR_TIM_HANDLE, 0);

    /* 发送 ≥10μs TRIG 高电平 (PC0) */
    HAL_GPIO_WritePin(RADAR_TRIG_PORT, RADAR_TRIG_PIN, GPIO_PIN_SET);
    Radar_Delay_15us();
    HAL_GPIO_WritePin(RADAR_TRIG_PORT, RADAR_TRIG_PIN, GPIO_PIN_RESET);

    s->last_trigger_tick = HAL_GetTick();

    /* 超时等待 */
    while (HAL_GetTick() - s->last_trigger_tick < (RADAR_TIMEOUT_US / 1000UL + 10)) {
        if (s->data_ready) return RADAR_OK;
    }

    s->timeout_flag  = 1;
    s->capture_state = RADAR_CAPTURE_IDLE;
    return RADAR_TIMEOUT;
}


/*----------------------------------------------------------------------------
 * 脉冲宽度读取
 *----------------------------------------------------------------------------*/
uint32_t Radar_Get_PulseWidth_us(void)
{
    return g_sensor.pulse_width_us;
}


/*----------------------------------------------------------------------------
 * 距离换算
 * distance_cm = pulse_width_us × sound_speed_cm_per_us / 2
 *----------------------------------------------------------------------------*/
float Radar_UltrasonicToDistance(uint32_t pulse_width_us)
{
    if (pulse_width_us == 0) return 0.0f;
    float dist = (float)pulse_width_us * g_sound_speed_cm_us * 0.5f;
    if (dist < RADAR_MIN_DISTANCE_CM) dist = RADAR_MIN_DISTANCE_CM;
    if (dist > RADAR_MAX_DISTANCE_CM) dist = RADAR_MAX_DISTANCE_CM;
    return dist;
}


/*----------------------------------------------------------------------------
 * 状态查询
 *----------------------------------------------------------------------------*/
uint8_t Radar_Is_Busy(void)
{
    return ((HAL_GetTick() - g_sensor.last_trigger_tick) < RADAR_MIN_INTERVAL_MS) ? 1 : 0;
}

uint8_t Radar_Is_DataReady(void)
{
    return g_sensor.data_ready;
}

void Radar_Clear_DataReady(void)
{
    g_sensor.data_ready = 0;
}


/*----------------------------------------------------------------------------
 * 温度补偿
 * 声速公式：c(m/s) = 331.4 + 0.607 × T(°C)，转换为 cm/μs
 *----------------------------------------------------------------------------*/
void Radar_Set_Temperature(float temp_celsius)
{
    float speed_m_s   = 331.4f + 0.607f * temp_celsius;
    g_sound_speed_cm_us = speed_m_s * 0.0001f;
}


/*----------------------------------------------------------------------------
 * 定时器溢出中断回调
 * 需在 HAL_TIM_PeriodElapsedCallback 中调用
 *----------------------------------------------------------------------------*/
void Radar_TIM_PeriodElapsed_Callback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == RADAR_TIM_HANDLE->Instance) {
        if (g_sensor.capture_state == RADAR_CAPTURE_GOT_RISE) {
            g_sensor.overflow_count++;
            if (g_sensor.overflow_count > 2) {
                g_sensor.timeout_flag  = 1;
                g_sensor.capture_state = RADAR_CAPTURE_IDLE;
            }
        }
    }
}


/*----------------------------------------------------------------------------
 * 输入捕获中断回调
 * 由 HAL_TIM_IRQHandler 自动调用
 *
 * TIM3_CH1 (PC6) 双边沿捕获：
 *   上升沿 → 记录计数，切换为下降沿
 *   下降沿 → 计算脉宽，标记数据就绪
 *----------------------------------------------------------------------------*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != RADAR_TIM_HANDLE->Instance) return;

    if (htim->Channel == RADAR_TIM_ACTIVE_CH) {

        if (g_sensor.capture_state == RADAR_CAPTURE_WAIT_RISE) {
            /* 上升沿 */
            g_sensor.rise_tick = HAL_TIM_ReadCapturedValue(htim, RADAR_TIM_CHANNEL);
            g_sensor.capture_state = RADAR_CAPTURE_GOT_RISE;

            __HAL_TIM_SET_CAPTUREPOLARITY(htim, RADAR_TIM_CHANNEL,
                                          TIM_INPUTCHANNELPOLARITY_FALLING);

        } else if (g_sensor.capture_state == RADAR_CAPTURE_GOT_RISE) {
            /* 下降沿 */
            uint32_t fall_tick = HAL_TIM_ReadCapturedValue(htim, RADAR_TIM_CHANNEL);

            if (fall_tick >= g_sensor.rise_tick) {
                g_sensor.pulse_width_us = (fall_tick - g_sensor.rise_tick);
            } else {
                /* TIM3 ARR=19999, 溢出点=20000, 不再使用 0xFFFF */
                g_sensor.pulse_width_us = (19999 - g_sensor.rise_tick) + fall_tick + 1;
            }

            g_sensor.data_ready    = 1;
            g_sensor.capture_state = RADAR_CAPTURE_COMPLETE;

            __HAL_TIM_SET_CAPTUREPOLARITY(htim, RADAR_TIM_CHANNEL,
                                          TIM_INPUTCHANNELPOLARITY_RISING);
        }
    }
}