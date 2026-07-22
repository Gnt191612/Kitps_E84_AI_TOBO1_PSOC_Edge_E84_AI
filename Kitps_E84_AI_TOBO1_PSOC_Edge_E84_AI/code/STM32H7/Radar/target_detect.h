/**
 * @file    target_detect.h
 * @brief   目标检测与方位几何解算
 * 
 * 核心输出：1+3个目标候选 → TargetCandidate_t 数组
 * 后续供 Algorithm/kalman_filter.c 和 Algorithm/data_fusion.c 使用
 */

#ifndef __TARGET_DETECT_H
#define __TARGET_DETECT_H

#include "radar_filter.h"
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 宏定义
 *----------------------------------------------------------------------------*/
#define TARGET_MAX_PER_FRAME    3       /* 单帧最多检测3个目标 */
#define CLUSTER_EPS_CM          25.0f   /* DBSCAN邻域半径(cm) */
#define CLUSTER_MIN_POINTS      2       /* DBSCAN最小点形成簇（多传感器≥2点/帧时使用） */
#define SINGLE_POINT_CONFIDENCE 0.50f   /* 单点雷达默认置信度（无聚类时直接使用） */

/*----------------------------------------------------------------------------
 * 目标候选结构体
 *----------------------------------------------------------------------------*/
typedef struct {
    uint8_t  id;            /* 临时ID(1~3) */
    float    x_cm;          /* X坐标(cm) */
    float    y_cm;          /* Y坐标(cm) */
    float    distance_cm;   /* 径向距离(cm) */
    float    angle_deg;     /* 方位角(°) */
    float    confidence;    /* 置信度(0~1) */
    uint8_t  valid;         /* 1:有效 */
} TargetCandidate_t;

/*----------------------------------------------------------------------------
 * API
 *----------------------------------------------------------------------------*/
void     TargetDetect_Init(void);
uint8_t  TargetDetect_Cluster(RawPoint_t *points, uint8_t cnt, TargetCandidate_t *targets);
uint8_t  TargetDetect_Coordinate_Transform(TargetCandidate_t *targets, uint8_t cnt);

#ifdef __cplusplus
}
#endif

#endif /* __TARGET_DETECT_H */