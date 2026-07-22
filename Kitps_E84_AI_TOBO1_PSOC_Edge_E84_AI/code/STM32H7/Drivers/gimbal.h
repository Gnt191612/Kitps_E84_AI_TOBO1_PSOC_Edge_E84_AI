/**
 * @file    gimbal.h
 * @brief   三路相机云台舵机组（H7 直控，每路 Pan+Tilt）
 *
 * 芯片型号：STM32H743ZIT6
 * 共 3 组云台，每组 2 路 PWM（水平 Pan + 垂直 Tilt），共 6 路：
 *
 *   Gimbal 0 (ESP32-A OV2640):
 *     Pan  = PA1 → TIM2_CH2
 *     Tilt = PA2 → TIM2_CH3
 *
 *   Gimbal 1 (ESP32-B OV2640):
 *     Pan  = PA3 → TIM2_CH4
 *     Tilt = PC7 → TIM3_CH2
 *
 *   Gimbal 2 (PSE84E OV7675):
 *     Pan  = PB0 → TIM3_CH3
 *     Tilt = PB1 → TIM3_CH4
 *
 * 舵机规格：
 *   水平: 360° 连续旋转（非限位舵机）
 *   垂直: 270°（仅用 0°~180°，0°=朝天, 90°=平视, 180°=朝地）
 *
 * 注意：雷达旋转舵机由独立的 Drivers/servo.c 管理（TIM2_CH1 / PA0）
 */

#ifndef __GIMBAL_H
#define __GIMBAL_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 云台编号 */
typedef enum {
    GIMBAL_CAM0 = 0,    /* ESP32-A */
    GIMBAL_CAM1 = 1,    /* ESP32-B */
    GIMBAL_CAM2 = 2,    /* PSE84E + OV7675 */
    GIMBAL_COUNT
} GimbalId_t;

/*----------------------------------------------------------------------------
 * 引脚映射（CubeMX 中需对应配置）
 *----------------------------------------------------------------------------*/
/* Gimbal 0 — ESP32-A */
#define GIMBAL0_PAN_PORT    GPIOA
#define GIMBAL0_PAN_PIN     GPIO_PIN_1
#define GIMBAL0_PAN_TIM     TIM2
#define GIMBAL0_PAN_CH      TIM_CHANNEL_2
#define GIMBAL0_TILT_PORT   GPIOA
#define GIMBAL0_TILT_PIN    GPIO_PIN_2
#define GIMBAL0_TILT_TIM    TIM2
#define GIMBAL0_TILT_CH     TIM_CHANNEL_3

/* Gimbal 1 — ESP32-B */
#define GIMBAL1_PAN_PORT    GPIOA
#define GIMBAL1_PAN_PIN     GPIO_PIN_3
#define GIMBAL1_PAN_TIM     TIM2
#define GIMBAL1_PAN_CH      TIM_CHANNEL_4
#define GIMBAL1_TILT_PORT   GPIOC
#define GIMBAL1_TILT_PIN    GPIO_PIN_7
#define GIMBAL1_TILT_TIM    TIM3
#define GIMBAL1_TILT_CH     TIM_CHANNEL_2

/* Gimbal 2 — PSE84E + OV7675 */
#define GIMBAL2_PAN_PORT    GPIOB
#define GIMBAL2_PAN_PIN     GPIO_PIN_0
#define GIMBAL2_PAN_TIM     TIM3
#define GIMBAL2_PAN_CH      TIM_CHANNEL_3
#define GIMBAL2_TILT_PORT   GPIOB
#define GIMBAL2_TILT_PIN    GPIO_PIN_1
#define GIMBAL2_TILT_TIM    TIM3
#define GIMBAL2_TILT_CH     TIM_CHANNEL_4

/*----------------------------------------------------------------------------
 * PWM 参数（与 servo.h 一致：PSC=199, ARR=19999, 50Hz）
 *----------------------------------------------------------------------------*/
#define GIMBAL_PWM_PSC      199
#define GIMBAL_PWM_ARR      19999
#define GIMBAL_PWM_FREQ     50

/* 360° 水平舵机：CCR 值映射到角度（0=极左, 180=极右, 后360°通过方向控制） */
#define GIMBAL_PAN_0DEG     500     /* 对应 ~0° */
#define GIMBAL_PAN_90DEG    1500    /* 对应 ~90° */
#define GIMBAL_PAN_180DEG   2500    /* 对应 ~180° */

/* 270° 垂直舵机：CCR 映射，仅用 0°~180° */
#define GIMBAL_TILT_0DEG    500     /* 朝上（极限） */
#define GIMBAL_TILT_90DEG   1500    /* 水平 */
#define GIMBAL_TILT_180DEG  2500    /* 朝下（极限） */

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/

/**
 * @brief 初始化所有云台舵机 PWM
 */
void Gimbal_Init(void);

/**
 * @brief 设置云台水平角
 * @param gimbal 云台编号 0/1/2
 * @param angle  角度(°), 范围 0~180
 */
void Gimbal_SetPan(GimbalId_t gimbal, float angle);

/**
 * @brief 设置云台垂直角
 * @param gimbal 云台编号 0/1/2
 * @param angle  角度(°), 范围 0~180（90=水平）
 */
void Gimbal_SetTilt(GimbalId_t gimbal, float angle);

/**
 * @brief 设置云台指向（同时设置 Pan+Tilt）
 * @param gimbal  云台编号
 * @param pan_deg 水平角
 * @param tilt_deg 垂直角
 */
void Gimbal_Point(GimbalId_t gimbal, float pan_deg, float tilt_deg);

/**
 * @brief 所有云台指向同一方向（多视角同步指向）
 * @param pan_deg  水平角
 * @param tilt_deg 垂直角
 */
void Gimbal_PointAll(float pan_deg, float tilt_deg);

/**
 * @brief 所有云台归中（Pan=90, Tilt=90）
 */
void Gimbal_CenterAll(void);

#ifdef __cplusplus
}
#endif

#endif /* __GIMBAL_H */
