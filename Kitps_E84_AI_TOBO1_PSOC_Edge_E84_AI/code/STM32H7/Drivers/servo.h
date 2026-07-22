/**
 * @file    servo.h
 * @brief   舵机驱动（用于平台旋转）
 *          芯片型号：STM32H743ZIT6
 *          引脚：PA0 → TIM2_CH1
 *
 * 舵机规格（SG90 或类似）：
 *   PWM: 50Hz (20ms)
 *   0°:   0.5ms 脉冲 (CCR=500)
 *   90°:  1.5ms 脉冲 (CCR=1500)
 *   180°: 2.5ms 脉冲 (CCR=2500)
 *
 * 物理安装：
 *   舵机驱动云台旋转，云台承载 HC-SR04 + 2×ESP32+OV2640 + PSE84E
 *   0° 为正前方，顺时针为正方向
 */

#ifndef __SERVO_H
#define __SERVO_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 硬件引脚映射
 *----------------------------------------------------------------------------*/
#define SERVO_TIM_HANDLE        (&htim2)       /* TIM2 */
#define SERVO_TIM_CHANNEL       TIM_CHANNEL_1  /* CH1 → PA0 */
#define SERVO_GPIO_PORT         GPIOA
#define SERVO_GPIO_PIN          GPIO_PIN_0

/*----------------------------------------------------------------------------
 * PWM 参数（TIM2: 200MHz, PSC=199 → 1MHz计数, ARR=19999 → 50Hz）
 *----------------------------------------------------------------------------*/
#define SERVO_TIM_PSC           199            /* 200MHz / 200 = 1MHz */
#define SERVO_TIM_ARR           19999          /* 1MHz / 20000 = 50Hz */

/* 脉宽 -> CCR 映射（1MHz = 1μs/计数值） */
#define SERVO_PULSE_0DEG        500            /* 0.5ms → 0° */
#define SERVO_PULSE_90DEG       1500           /* 1.5ms → 90° */
#define SERVO_PULSE_180DEG      2500           /* 2.5ms → 180° */

/*----------------------------------------------------------------------------
 * 角度范围与限幅
 *----------------------------------------------------------------------------*/
#define SERVO_ANGLE_MIN         0.0f           /* 最小角度(°) */
#define SERVO_ANGLE_MAX         180.0f         /* 最大角度(°) */
#define SERVO_ANGLE_CENTER      90.0f          /* 中位(正前方) */

/* ──── 连续旋转舵机扫描参数 ──── */
/* 雷达舵机为 360° 连续旋转舵机（非标准位置舵机）。
 * 脉冲宽度控制旋转方向与速度：
 *   500μs  (0°)   → 全速左转
 *   1500μs (90°)  → 停止
 *   2500μs (180°) → 全速右转
 *
 * 扫描时序：左转180° → 右转180° → 右转再180° → 左转180° → 循环
 */
#define SERVO_STOP_CCR          1500           /* 停止脉冲(μs) */
#define SERVO_FULLLEFT_CCR      500            /* 全速左转脉冲(μs) */
#define SERVO_FULLRIGHT_CCR     2500           /* 全速右转脉冲(μs) */

/* 舵机全速转 180° 所需时间(ms) — 实物校准时修改此值 */
#define SERVO_TIME_20MS         111           /* 全速转 180° 耗时(ms) */

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/

/**
 * @brief 初始化舵机 PWM（CubeMX 配置 TIM2 后调用此补充初始化）
 */
void Servo_Init(void);

/**
 * @brief 设置舵机角度
 * @param angle 目标角度(°)，范围 0~180
 */
void Servo_SetAngle(float angle);

/**
 * @brief 连续旋转舵机直接控制（跳过角度映射）
 * @param ccr 直接设 CCR 脉冲值(μs)
 *   500  → 全速左转 | 1500 → 停止 | 2500 → 全速右转
 */
void Servo_SetDirect(uint32_t ccr);

/**
 * @brief 获取当前角度
 * @return 当前角度(°)
 */
float Servo_GetAngle(void);

/**
 * @brief 舵机扫描（从 start 到 end，步进 step_deg）
 * @param start_deg   起始角度(°)
 * @param end_deg     终止角度(°)
 * @param step_deg    步进角度(°)
 * @param delay_ms    每步停留时间(ms)
 * @param callback    每步回调，传入当前角度；NULL 则仅延时
 *                    回调返回 1 时终止扫描
 */
void Servo_Sweep(float start_deg, float end_deg, float step_deg,
                 uint32_t delay_ms, int (*callback)(float angle));

/**
 * @brief 回到中位（正前方 90°）
 */
void Servo_Center(void);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_H */
