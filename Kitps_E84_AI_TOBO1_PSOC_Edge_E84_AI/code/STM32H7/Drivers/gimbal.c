/**
 * @file    gimbal.c
 * @brief   三路相机云台舵机组实现
 */

#include "gimbal.h"
#include "tim.h"

static uint8_t s_initialized = 0;

static const struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
} s_pan_tbl[GIMBAL_COUNT] = {
    { &htim2, TIM_CHANNEL_2 },   /* GIMBAL_CAM0 Pan: PA1 */
    { &htim2, TIM_CHANNEL_4 },   /* GIMBAL_CAM1 Pan: PA3 */
    { &htim3, TIM_CHANNEL_3 },   /* GIMBAL_CAM2 Pan: PB0 */
};
static const struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
} s_tilt_tbl[GIMBAL_COUNT] = {
    { &htim2, TIM_CHANNEL_3 },   /* GIMBAL_CAM0 Tilt: PA2 */
    { &htim3, TIM_CHANNEL_2 },   /* GIMBAL_CAM1 Tilt: PC7 */
    { &htim3, TIM_CHANNEL_4 },   /* GIMBAL_CAM2 Tilt: PB1 */
};

static uint32_t AngleToCCR(float angle, uint32_t ccr_min, uint32_t ccr_max)
{
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    float ccr = ccr_min + (angle / 180.0f) * (ccr_max - ccr_min);
    return (uint32_t)(ccr + 0.5f);
}

void Gimbal_Init(void)
{
    if (s_initialized) return;

    s_initialized = 1;

    for (int i = 0; i < GIMBAL_COUNT; i++) {
        HAL_TIM_PWM_Start(s_pan_tbl[i].htim, s_pan_tbl[i].channel);
        HAL_TIM_PWM_Start(s_tilt_tbl[i].htim, s_tilt_tbl[i].channel);
    }

    Gimbal_CenterAll();
}

void Gimbal_SetPan(GimbalId_t gimbal, float angle)
{
    if (gimbal >= GIMBAL_COUNT) return;
    if (!s_initialized) Gimbal_Init();
    uint32_t ccr = AngleToCCR(angle, GIMBAL_PAN_0DEG, GIMBAL_PAN_180DEG);
    __HAL_TIM_SET_COMPARE(s_pan_tbl[gimbal].htim, s_pan_tbl[gimbal].channel, ccr);
}

void Gimbal_SetTilt(GimbalId_t gimbal, float angle)
{
    if (gimbal >= GIMBAL_COUNT) return;
    if (!s_initialized) Gimbal_Init();
    uint32_t ccr = AngleToCCR(angle, GIMBAL_TILT_0DEG, GIMBAL_TILT_180DEG);
    __HAL_TIM_SET_COMPARE(s_tilt_tbl[gimbal].htim, s_tilt_tbl[gimbal].channel, ccr);
}

void Gimbal_Point(GimbalId_t gimbal, float pan_deg, float tilt_deg)
{
    Gimbal_SetPan(gimbal, pan_deg);
    Gimbal_SetTilt(gimbal, tilt_deg);
}

void Gimbal_PointAll(float pan_deg, float tilt_deg)
{
    for (int i = 0; i < GIMBAL_COUNT; i++) {
        Gimbal_Point((GimbalId_t)i, pan_deg, tilt_deg);
    }
}

void Gimbal_CenterAll(void)
{
    Gimbal_PointAll(90.0f, 90.0f);
}
