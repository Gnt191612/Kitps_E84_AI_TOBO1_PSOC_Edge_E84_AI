/**
 * @file    radar_driver.h
 * @brief   HC-SR04超声波硬件驱动接口（非阻塞）
 *          芯片型号：STM32H743ZIT6
 *
 *          连线方案：
 *          TRIG -> PC0 (GPIO推挽输出)
 *          ECHO -> PC6 (TIM3_CH1 输入捕获)
 *
 * 用法：Radar_Trigger() 只发出 TRIG 脉冲后立即返回；回波由 TIM3_CH1
 * 输入捕获中断记录；主循环用 Radar_Poll() 取结果，不忙等。
 *
 * TIM3 同时输出三路云台舵机 PWM（CH2/3/4），驱动不得改写 TIM3 计数器。
 * 脉宽 = 两次捕获值之差（模 ARR+1）+ 由毫秒时基判定的整周期数。
 *
 * @note    ECHO引脚需用2.2kΩ+3.3kΩ分压将5V降至3.3V再接入MCU！
 */

#ifndef __RADAR_DRIVER_H
#define __RADAR_DRIVER_H

#include "main.h"
#include "tim.h"
#include "gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 硬件引脚映射（与CubeMX生成的gpio.h/tim.h保持一致）
 *----------------------------------------------------------------------------*/
#define RADAR_TRIG_PORT         GPIOC
#define RADAR_TRIG_PIN          GPIO_PIN_0      /* PC0 */
#define RADAR_ECHO_PORT         GPIOC
#define RADAR_ECHO_PIN          GPIO_PIN_6      /* PC6 */

/* TIM3 硬件句柄 */
#define RADAR_TIM_HANDLE        (&htim3)
#define RADAR_TIM_CHANNEL       TIM_CHANNEL_1   /* PC6 -> TIM3_CH1 */
#define RADAR_TIM_ACTIVE_CH     HAL_TIM_ACTIVE_CHANNEL_1

/* 定时器计数频率 = 200MHz / (Prescaler+1) = 200MHz / 200 = 1MHz */
#define RADAR_TIM_FREQ_HZ       1000000UL       /* 1MHz，即1个计数 = 1μs */
#define RADAR_TIM_PERIOD_US     20000UL         /* TIM3 ARR+1（50Hz 舵机 PWM） */

/*----------------------------------------------------------------------------
 * 测量参数宏定义
 *----------------------------------------------------------------------------*/
#define RADAR_MIN_INTERVAL_MS   60              /* 两次测量最小安全间隔(ms)，避免超声串扰 */
#define RADAR_SOUND_SPEED_CM_US 0.0343f         /* 声速(cm/us) @20°C */
#define RADAR_MAX_DISTANCE_CM   400.0f          /* HC-SR04最大有效测距距离(cm) */
#define RADAR_MIN_DISTANCE_CM   2.0f            /* HC-SR04最小有效测距距离(cm) */
#define RADAR_ECHO_TIMEOUT_MS   40U             /* 触发后无完整回波的超时(ms) */

/*----------------------------------------------------------------------------
 * 枚举类型
 *----------------------------------------------------------------------------*/
typedef enum {
    RADAR_OK        = 0x00U,   /* 已触发 */
    RADAR_TIMEOUT   = 0x01U,   /* 超时(无回波/目标过远) */
    RADAR_BUSY      = 0x02U,   /* 传感器忙，测量进行中或未到最小间隔 */
    RADAR_NO_INIT   = 0x03U,   /* 驱动未初始化 */
    RADAR_FAULT     = 0x04U    /* 硬件故障 */
} RadarStatus_t;

typedef enum {
    RADAR_RESULT_NONE = 0,     /* 无新结果（空闲或仍在等待回波） */
    RADAR_RESULT_OK,           /* 测得有效距离 */
    RADAR_RESULT_NO_ECHO       /* 超时或超出量程：该方向在量程内没有物体 */
} RadarResult_t;

typedef enum {
    RADAR_CAPTURE_IDLE = 0x00U, /* 空闲，等待触发 */
    RADAR_CAPTURE_WAIT_RISE,    /* 已触发，等待上升沿 */
    RADAR_CAPTURE_GOT_RISE,     /* 已捕获上升沿，等待下降沿 */
    RADAR_CAPTURE_COMPLETE      /* 捕获完成，等待主循环取走 */
} RadarCaptureState_t;

/*----------------------------------------------------------------------------
 * 传感器数据结构
 *----------------------------------------------------------------------------*/
typedef struct {
    volatile uint32_t    pulse_width_us;  /* 最近一次测量的高电平脉宽(μs) */
    volatile uint32_t    rise_cap;        /* 上升沿捕获值 */
    volatile uint32_t    rise_ms;         /* 上升沿时刻(HAL_GetTick) */
    volatile RadarCaptureState_t capture_state;
    uint32_t             last_trigger_tick; /* 上次触发时刻(HAL_GetTick) */
    uint8_t              sensor_id;       /* 传感器编号 */
} RadarSensor_t;

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/
void          Radar_Driver_Init(void);
/* 发出一次 TRIG；返回 RADAR_OK 表示已触发，RADAR_BUSY 表示上次测量未结束或间隔不足 */
RadarStatus_t Radar_Trigger(void);
/* 取测量结果；RADAR_RESULT_OK 时 *dist_cm 有效 */
RadarResult_t Radar_Poll(float *dist_cm);
uint8_t       Radar_Is_Busy(void);
float         Radar_UltrasonicToDistance(uint32_t pulse_width_us);

/* 温度补偿 */
void          Radar_Set_Temperature(float temp_celsius);

/* 输入捕获回调（由HAL_TIM_IC_CaptureCallback自动调用，无需手动注册） */
void          HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif /* __RADAR_DRIVER_H */
