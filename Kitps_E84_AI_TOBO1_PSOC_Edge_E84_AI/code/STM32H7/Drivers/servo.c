/**
 * @file    servo.c
 * @brief   舵机驱动实现
 *          使用 TIM2_CH1 (PA0) 输出 50Hz PWM 控制舵机角度
 */

#include "servo.h"
#include "tim.h"
#include <math.h>

/*----------------------------------------------------------------------------
 * 内部状态
 *----------------------------------------------------------------------------*/
static float s_current_angle = SERVO_ANGLE_CENTER;
static uint8_t s_initialized = 0;

/*----------------------------------------------------------------------------
 * 辅助：角度 → CCR
 * 线性映射：0° → 500, 180° → 2500
 *----------------------------------------------------------------------------*/
static uint32_t AngleToCCR(float angle)
{
    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    /* 映射 0°~180° → 500~2500 */
    float ccr = SERVO_PULSE_0DEG
              + (angle / (SERVO_ANGLE_MAX - SERVO_ANGLE_MIN))
              * (SERVO_PULSE_180DEG - SERVO_PULSE_0DEG);
    return (uint32_t)(ccr + 0.5f);
}

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void Servo_Init(void)
{
    if (s_initialized) return;

    /* CubeMX 已配置 TIM2 基本参数，这里做 PWM 输出启动 */
    HAL_TIM_PWM_Start(SERVO_TIM_HANDLE, SERVO_TIM_CHANNEL);

    /* 回到中位 */
    s_current_angle = SERVO_ANGLE_CENTER;
    uint32_t ccr = AngleToCCR(s_current_angle);
    __HAL_TIM_SET_COMPARE(SERVO_TIM_HANDLE, SERVO_TIM_CHANNEL, ccr);

    s_initialized = 1;
}

/*----------------------------------------------------------------------------
 * 设置角度
 *----------------------------------------------------------------------------*/
void Servo_SetAngle(float angle)
{
    if (!s_initialized) Servo_Init();

    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    uint32_t ccr = AngleToCCR(angle);
    __HAL_TIM_SET_COMPARE(SERVO_TIM_HANDLE, SERVO_TIM_CHANNEL, ccr);
    s_current_angle = angle;
}

float Servo_GetAngle(void)
{
    return s_current_angle;
}

/*----------------------------------------------------------------------------
 * 连续旋转舵机直接控制（跳过角度映射，直接设 CCR 脉冲宽度）
 * 用于 360° 连续旋转舵机（速度控制）
 *   ccr=500 → 全速左转
 *   ccr=1500 → 停止
 *   ccr=2500 → 全速右转
 *----------------------------------------------------------------------------*/
void Servo_SetDirect(uint32_t ccr)
{
    if (!s_initialized) Servo_Init();
    __HAL_TIM_SET_COMPARE(SERVO_TIM_HANDLE, SERVO_TIM_CHANNEL, ccr);
}

/*----------------------------------------------------------------------------
 * 回到中位
 *----------------------------------------------------------------------------*/
void Servo_Center(void)
{
    Servo_SetAngle(SERVO_ANGLE_CENTER);
}

/*----------------------------------------------------------------------------
 * 扫描
 * 从 start_deg 到 end_deg 步进，每步调用 callback（如果有）并延时
 * callback 返回 1 时提前终止
 *----------------------------------------------------------------------------*/
void Servo_Sweep(float start_deg, float end_deg, float step_deg,
                 uint32_t delay_ms, int (*callback)(float angle))
{
    float angle = start_deg;
    int direction = (end_deg > start_deg) ? 1 : -1;

    while (1) {
        /* 检查是否越过终止边界 */
        if (direction > 0 && angle > end_deg) break;
        if (direction < 0 && angle < end_deg) break;

        Servo_SetAngle(angle);

        /* 回调 */
        if (callback) {
            if (callback(angle)) break;  /* callback 返回 1 → 终止 */
        }

        /* 等待舵机到位 + 传感器稳定 */
        if (delay_ms > 0) {
            HAL_Delay(delay_ms);
        }

        angle += direction * step_deg;
    }
}
