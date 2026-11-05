/**
 * @file    radar_process.h
 * @brief   雷达测量调度与候选目标输出（非阻塞）
 *
 * 职责：
 *   - 按固定间隔触发 HC-SR04，轮询回波，不阻塞主循环
 *   - 把每次测量连同测量时的舵机角记入距离图（radar_map.c）
 *   - 背景学好后，输出“比背景明显变近”的候选目标
 *
 * 安装位置：
 *   雷达点位于中心点 (0, 0, 0) 的 Y 轴正方向 0.2m 处，
 *   与两个跟踪点 (ESP32) 处于同一基平面 (Z=0)。
 *   坐标见 config_mount.h: DEVICE_RADAR_*。
 */

#ifndef __RADAR_PROCESS_H
#define __RADAR_PROCESS_H

#include "radar_driver.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 宏定义
 *----------------------------------------------------------------------------*/
#define RADAR_PING_INTERVAL_MS  70U     /* 触发间隔（≥ RADAR_MIN_INTERVAL_MS） */

struct TargetCandidate_s;

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/
void     Radar_Process_Init(void);
/* 每个主循环调用一次。产生新测量且该处形成候选时返回 1 并填写 *out */
uint8_t  Radar_Process_Update(struct TargetCandidate_s *out);
/* 背景是否已学好（学好之前不会输出候选） */
uint8_t  Radar_Process_BackgroundReady(void);
/* 84E 判为非人体：把该方位附近当前距离并入背景 */
void     Radar_Process_Absorb(float bearing_deg);
/* 丢弃背景并重新学习（现场换环境时用），期间不输出候选 */
void     Radar_Process_Relearn(void);

#ifdef __cplusplus
}
#endif

#endif /* __RADAR_PROCESS_H */
