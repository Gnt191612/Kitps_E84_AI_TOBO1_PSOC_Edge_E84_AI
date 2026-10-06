/**
 * @file closed_loop.c
 * @brief 跟踪闭环 PID 控制实现
 */

#include "closed_loop.h"
#include "data_logger/logger.h"

/* ──── PID 参数 ──── */
#define DEFAULT_KP  0.5f
#define DEFAULT_KI  0.01f
#define DEFAULT_KD  0.1f

/* 视野中心 (期望目标位置) */
#define CENTER_X_MM  150.0f   /* 300mm / 2 */
#define CENTER_Y_MM  112.5f   /* 225mm / 2 */

/* 控制量限幅 */
#define CTRL_MAX  400

/* ──── PID 状态 ──── */
static struct {
    float kp, ki, kd;
    float integral_x, integral_y;
    float prev_err_x, prev_err_y;
    float output_x, output_y;
    int initialized;
} s_pid;

/* ──── 初始化 ──── */
void ClosedLoop_Init(void)
{
    s_pid.kp = DEFAULT_KP;
    s_pid.ki = DEFAULT_KI;
    s_pid.kd = DEFAULT_KD;
    s_pid.integral_x = 0;
    s_pid.integral_y = 0;
    s_pid.prev_err_x = 0;
    s_pid.prev_err_y = 0;
    s_pid.output_x = 0;
    s_pid.output_y = 0;
    s_pid.initialized = 1;

    LOG_INFO("ClosedLoop init: Kp=%.2f, Ki=%.2f, Kd=%.2f", DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);
}

/* ──── PID 更新 ──── */
void ClosedLoop_Update(float target_x, float target_y)
{
    if (!s_pid.initialized) ClosedLoop_Init();

    /* 计算误差 (目标位置 - 视野中心) */
    float err_x = target_x - CENTER_X_MM;
    float err_y = target_y - CENTER_Y_MM;

    /* 积分 */
    s_pid.integral_x += err_x;
    s_pid.integral_y += err_y;

    /* 积分限幅 */
    const float I_LIMIT = 200;
    if (s_pid.integral_x > I_LIMIT) s_pid.integral_x = I_LIMIT;
    if (s_pid.integral_x < -I_LIMIT) s_pid.integral_x = -I_LIMIT;
    if (s_pid.integral_y > I_LIMIT) s_pid.integral_y = I_LIMIT;
    if (s_pid.integral_y < -I_LIMIT) s_pid.integral_y = -I_LIMIT;

    /* 微分 */
    float deriv_x = err_x - s_pid.prev_err_x;
    float deriv_y = err_y - s_pid.prev_err_y;

    /* PID 输出 */
    float out_x = s_pid.kp * err_x + s_pid.ki * s_pid.integral_x + s_pid.kd * deriv_x;
    float out_y = s_pid.kp * err_y + s_pid.ki * s_pid.integral_y + s_pid.kd * deriv_y;

    /* 限幅 */
    if (out_x > CTRL_MAX) out_x = CTRL_MAX;
    if (out_x < -CTRL_MAX) out_x = -CTRL_MAX;
    if (out_y > CTRL_MAX) out_y = CTRL_MAX;
    if (out_y < -CTRL_MAX) out_y = -CTRL_MAX;

    s_pid.output_x = out_x;
    s_pid.output_y = out_y;
    s_pid.prev_err_x = err_x;
    s_pid.prev_err_y = err_y;

    /* ESP32只计算控制量并上报，舵机PWM统一由H7输出。 */
}

/* ──── 设置 PID ──── */
void ClosedLoop_SetPID(float kp, float ki, float kd)
{
    s_pid.kp = kp;
    s_pid.ki = ki;
    s_pid.kd = kd;
}

/* ──── 获取输出 ──── */
void ClosedLoop_GetOutput(float *pan, float *tilt)
{
    if (pan) *pan = s_pid.output_x;
    if (tilt) *tilt = s_pid.output_y;
}

/* ──── 重置 ──── */
void ClosedLoop_Reset(void)
{
    s_pid.integral_x = 0;
    s_pid.integral_y = 0;
    s_pid.prev_err_x = 0;
    s_pid.prev_err_y = 0;
    s_pid.output_x = 0;
    s_pid.output_y = 0;
}
