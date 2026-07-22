/**
 * @file    radar_process.h
 * @brief   雷达点云解析与0.2m目标唤醒判断
 * 
 * 职责：
 *   - 采集原始测量数据，构建RawPoint_t点云
 *   - 判断0.2m外是否存在有效目标，触发NPU扫描
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
#define RAW_POINTS_MAX          16      /* 单帧最大原始点数量 */
#define WAKE_DISTANCE_CM_MIN    20.0f   /* 0.2m下限 */
#define WAKE_DISTANCE_CM_MAX    400.0f  /* 有效识别距离上限(4m) */
#define RADAR_SAMPLE_COUNT      5       /* 滑动窗口大小，用于去抖 */

/*----------------------------------------------------------------------------
 * 数据类型
 *----------------------------------------------------------------------------*/
typedef struct {
    float    distance_cm;    /* 距离(cm) */
    float    angle_deg;      /* 方位角(°)，单支传感器默认为0，双传感器可通过几何解算获取 */
    uint8_t  sensor_id;      /* 0:主传感器, 1:辅助传感器 */
    uint8_t  valid;          /* 1:数据有效 */
} RawPoint_t;

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/
void     Radar_Process_Init(void);
uint8_t  Radar_Process_Scan(RawPoint_t *out_points, uint8_t max_num);

/* 0.2m 目标唤醒判断（去抖处理：连续5帧中≥3帧存在目标才确认唤醒） */
uint8_t  Radar_Is_Target_Wake(void);
void     Radar_Update_WakeBuffer(uint8_t has_target);
uint8_t  Radar_Is_Target_Within(float min_cm, float max_cm, RawPoint_t *points, uint8_t cnt);

#ifdef __cplusplus
}
#endif

#endif /* __RADAR_PROCESS_H */