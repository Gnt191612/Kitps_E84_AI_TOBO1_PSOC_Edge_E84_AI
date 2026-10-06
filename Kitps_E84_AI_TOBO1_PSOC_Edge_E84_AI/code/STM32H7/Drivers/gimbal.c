/**
 * @file    gimbal.c
 * @brief   三路相机云台舵机组实现
 */

#include "gimbal.h"
#include "tim.h"

static uint8_t s_initialized = 0;
static float s_tilt_angle[GIMBAL_COUNT] = {90.0f, 90.0f, 90.0f};
static float s_pan_angle[GIMBAL_COUNT] = {135.0f, 135.0f, 135.0f};
static float s_pan_target[GIMBAL_COUNT] = {135.0f, 135.0f, 135.0f};
static uint32_t s_pan_deadline[GIMBAL_COUNT] = {0U, 0U, 0U};
static uint8_t s_pan_moving[GIMBAL_COUNT] = {0U, 0U, 0U};

static float ClampPanAngle(float angle)
{
    if (angle < GIMBAL_PAN_HARD_MIN) angle = GIMBAL_PAN_HARD_MIN;
    if (angle > GIMBAL_PAN_HARD_MAX) angle = GIMBAL_PAN_HARD_MAX;
    return angle;
}

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

    /* 先写入初始姿态，再启动 PWM，避免上电瞬间输出旧 CCR。 */
    for (int i = 0; i < GIMBAL_COUNT; i++) {
        __HAL_TIM_SET_COMPARE(s_pan_tbl[i].htim, s_pan_tbl[i].channel,
                              GIMBAL_PAN_135DEG);
        __HAL_TIM_SET_COMPARE(s_tilt_tbl[i].htim, s_tilt_tbl[i].channel,
                              GIMBAL_TILT_90DEG);
        HAL_TIM_PWM_Start(s_pan_tbl[i].htim, s_pan_tbl[i].channel);
        HAL_TIM_PWM_Start(s_tilt_tbl[i].htim, s_tilt_tbl[i].channel);
    }

    Gimbal_CenterAll();
}

void Gimbal_SetPan(GimbalId_t gimbal, float angle)
{
    if (gimbal >= GIMBAL_COUNT) return;
    if (!s_initialized) Gimbal_Init();
    angle = ClampPanAngle(angle);
    uint32_t ccr = (uint32_t)(GIMBAL_PAN_0DEG +
        (angle / 270.0f) * (GIMBAL_PAN_270DEG - GIMBAL_PAN_0DEG) + 0.5f);
    __HAL_TIM_SET_COMPARE(s_pan_tbl[gimbal].htim, s_pan_tbl[gimbal].channel, ccr);
    s_pan_angle[gimbal] = angle;
    s_pan_target[gimbal] = angle;
}

void Gimbal_SetPanMotion(GimbalId_t gimbal, GimbalPanMotion_t motion)
{
    if (gimbal >= GIMBAL_COUNT) return;
    if (!s_initialized) Gimbal_Init();

    float angle = s_pan_angle[gimbal];
    if (motion == GIMBAL_PAN_LEFT) angle -= 1.0f;
    if (motion == GIMBAL_PAN_RIGHT) angle += 1.0f;
    Gimbal_SetPan(gimbal, angle);
}

void Gimbal_ApplyTrackingControl(GimbalId_t gimbal,
                                 float pan_ctrl, float tilt_ctrl)
{
    if (gimbal >= GIMBAL_COUNT) return;
    if (!s_initialized) Gimbal_Init();

    s_pan_moving[gimbal] = 0U;
    if (pan_ctrl > GIMBAL_CTRL_INPUT_MAX) pan_ctrl = GIMBAL_CTRL_INPUT_MAX;
    if (pan_ctrl < -GIMBAL_CTRL_INPUT_MAX) pan_ctrl = -GIMBAL_CTRL_INPUT_MAX;
    if (tilt_ctrl > GIMBAL_CTRL_INPUT_MAX) tilt_ctrl = GIMBAL_CTRL_INPUT_MAX;
    if (tilt_ctrl < -GIMBAL_CTRL_INPUT_MAX) tilt_ctrl = -GIMBAL_CTRL_INPUT_MAX;

    if (pan_ctrl > GIMBAL_CTRL_DEADBAND || pan_ctrl < -GIMBAL_CTRL_DEADBAND) {
        Gimbal_SetPan(gimbal, s_pan_angle[gimbal] + pan_ctrl *
                      (GIMBAL_PAN_STEP_MAX / GIMBAL_CTRL_INPUT_MAX));
    }

    if (tilt_ctrl > GIMBAL_CTRL_DEADBAND || tilt_ctrl < -GIMBAL_CTRL_DEADBAND) {
        s_tilt_angle[gimbal] += tilt_ctrl *
                                (GIMBAL_TILT_STEP_MAX / GIMBAL_CTRL_INPUT_MAX);
        Gimbal_SetTilt(gimbal, s_tilt_angle[gimbal]);
    }
}

void Gimbal_MovePanTo(GimbalId_t gimbal, float angle_deg)
{
    if (gimbal >= GIMBAL_COUNT) return;
    if (!s_initialized) Gimbal_Init();

    float target = ClampPanAngle(angle_deg);
    float delta = target - s_pan_angle[gimbal];
    float abs_delta = (delta < 0.0f) ? -delta : delta;

    if (abs_delta < 1.0f) {
        Gimbal_SetPan(gimbal, target);
        s_pan_moving[gimbal] = 0U;
        return;
    }

    s_pan_target[gimbal] = target;
    s_pan_deadline[gimbal] = HAL_GetTick() +
        (uint32_t)(abs_delta * (float)GIMBAL_PAN_135_MOVE_MS / 135.0f);
    s_pan_moving[gimbal] = 1U;
    Gimbal_SetPan(gimbal, target);
}

void Gimbal_ReturnToInitial(GimbalId_t gimbal)
{
    Gimbal_MovePanTo(gimbal, GIMBAL_PAN_INITIAL_ANGLE);
    Gimbal_SetTilt(gimbal, 90.0f);
}

void Gimbal_Update(void)
{
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < GIMBAL_COUNT; i++) {
        if (s_pan_moving[i] && (int32_t)(now - s_pan_deadline[i]) >= 0) {
            s_pan_moving[i] = 0U;
        }
    }
}

uint8_t Gimbal_IsPanMoving(GimbalId_t gimbal)
{
    return (gimbal < GIMBAL_COUNT) ? s_pan_moving[gimbal] : 0U;
}

float Gimbal_GetEstimatedPan(GimbalId_t gimbal)
{
    return (gimbal < GIMBAL_COUNT) ? s_pan_angle[gimbal] : GIMBAL_PAN_INITIAL_ANGLE;
}

void Gimbal_SetTilt(GimbalId_t gimbal, float angle)
{
    if (gimbal >= GIMBAL_COUNT) return;
    if (!s_initialized) Gimbal_Init();
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    s_tilt_angle[gimbal] = angle;
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
    for (int i = 0; i < GIMBAL_COUNT; i++) {
        Gimbal_SetPan((GimbalId_t)i, GIMBAL_PAN_INITIAL_ANGLE);
        Gimbal_SetTilt((GimbalId_t)i, 90.0f);
    }
}
