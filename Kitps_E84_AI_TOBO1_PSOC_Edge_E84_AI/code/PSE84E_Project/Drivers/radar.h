/**
 * @file    radar.h
 * @brief   HC-SR04 雷达控制代理 — 通过 UART 与 STM32H7 通信
 *
 * HC-SR04 物理连接在 STM32H7 上，PSE84E 通过 UART 下发指令：
 *   - Radar_SetAngle(angle)   → H7 控制舵机旋转
 *   - Radar_GetEchoStrength() → H7 触发测距并回传结果
 */

#ifndef __RADAR_H
#define __RADAR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化雷达代理
 */
void Radar_Init(void);

/**
 * @brief 设置雷达扫描角度（通知 H7 控制舵机）
 * @param angle_deg 目标角度（度），范围 -45.0 ~ +45.0
 */
void Radar_SetAngle(float angle_deg);

/**
 * @brief 获取最后一次测距的回波强度（由 H7 回传）
 * @return 回波强度值 0.0~1.0，0 表示无回波
 *
 * 每次调用会通过 UART 请求 H7 执行一轮触发 → 测量 → 回传
 */
float Radar_GetEchoStrength(void);

/**
 * @brief 获取最近一次测量的距离（cm）
 */
float Radar_GetDistance(void);

#ifdef __cplusplus
}
#endif

#endif /* __RADAR_H */
