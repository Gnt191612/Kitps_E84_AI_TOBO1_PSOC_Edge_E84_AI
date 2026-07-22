/**
 * @file closed_loop.h
 * @brief 跟踪闭环：目标在视野内则维持控制量输出
 *
 * 使用 PID 控制输出云台 PWM 控制量。
 * 接口: ClosedLoop_Init, ClosedLoop_Update
 */

#ifndef CLOSED_LOOP_H
#define CLOSED_LOOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 公共接口 ──── */

/**
 * @brief 初始化闭环控制
 */
void ClosedLoop_Init(void);

/**
 * @brief 更新闭环控制
 * @param target_x 目标 X 坐标 (mm)
 * @param target_y 目标 Y 坐标 (mm)
 */
void ClosedLoop_Update(float target_x, float target_y);

/**
 * @brief 设置 PID 参数
 * @param kp 比例增益
 * @param ki 积分增益
 * @param kd 微分增益
 */
void ClosedLoop_SetPID(float kp, float ki, float kd);

/**
 * @brief 获取当前控制量
 * @param pan  输出水平控制量
 * @param tilt 输出垂直控制量
 */
void ClosedLoop_GetOutput(float *pan, float *tilt);

/**
 * @brief 重置闭环 (停止跟踪时调用)
 */
void ClosedLoop_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOSED_LOOP_H */
