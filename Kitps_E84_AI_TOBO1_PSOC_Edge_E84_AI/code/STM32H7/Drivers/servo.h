/**
 * @file    servo.h
 * @brief   舵机驱动（用于平台旋转）
 *          芯片型号：STM32H743ZIT6
 *          引脚：PA0 → TIM2_CH1
 *
 * 舵机规格：270°位置舵机
 *   PWM: 50Hz (20ms)
 *   当前仅使用 0.5/1.5/2.5ms 作为台架标定起点；实物安全端点待测。
 *
 * 物理安装：
 *   舵机驱动云台旋转，云台承载 HC-SR04 + 2×GOOUUU ESP32-S3-CAM N16R8/OV2640
 *   + KIT_PSE84_AI/PSE846GPS2DBZC4A/OV7675 DVP
 *   135° 为正前方；正方向需按实物安装方向校准
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

/* 脉宽 -> CCR 映射（1MHz = 1μs/计数值；均为待实物校准的起点） */
#define SERVO_PULSE_0DEG        500            /* 0.5ms → 0° */
#define SERVO_PULSE_CENTER      1500           /* 1.5ms → 135° */
#define SERVO_PULSE_270DEG      2500           /* 2.5ms → 270° */

/*----------------------------------------------------------------------------
 * 角度范围与限幅
 *----------------------------------------------------------------------------*/
#define SERVO_ANGLE_MIN         0.0f           /* 最小角度(°) */
#define SERVO_ANGLE_MAX         270.0f         /* 最大角度(°) */
#define SERVO_ANGLE_CENTER      135.0f         /* 中位(正前方) */

/* 扫描时序：135°→0°→135°→270°→135°，每段转动135°。 */
#define SERVO_SWEEP_LEG_MS      3000U          /* 每段扫描时间，需实物整定 */
#define SERVO_HOME_SETTLE_MS    2000U          /* 关机回正等待时间 */

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/

/**
 * @brief 初始化舵机 PWM（CubeMX 配置 TIM2 后调用此补充初始化）
 */
void Servo_Init(void);

/**
 * @brief 设置舵机角度
 * @param angle 目标角度(°)，范围 0~270
 */
void Servo_SetAngle(float angle);

void Servo_SetEstimatedAngle(float angle);

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
 * @brief 回到中位（正前方 135°）
 */
void Servo_Center(void);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_H */
