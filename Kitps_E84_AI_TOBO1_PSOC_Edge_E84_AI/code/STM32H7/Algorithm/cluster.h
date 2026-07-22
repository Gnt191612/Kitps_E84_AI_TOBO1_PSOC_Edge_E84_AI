/**
 * @file    cluster.h
 * @brief   DBSCAN 聚类算法
 *
 * 公开接口：
 *   Cluster_Init()                    - 初始化聚类模块
 *   Cluster_DBSCAN(points, cnt, targets) - 对雷达点云执行DBSCAN聚类，
 *                                          输出目标候选列表
 *
 * 算法参数（定义于 target_detect.h）：
 *   CLUSTER_EPS_CM     = 25.0f       - 邻域半径(cm)
 *   CLUSTER_MIN_POINTS = 2           - 最小邻域点数（含自身）
 *
 * 输入：RawPoint_t 数组（distance_cm, angle_deg, sensor_id, valid）
 * 输出：TargetCandidate_t 数组（id, x_cm, y_cm, distance_cm, angle_deg,
 *                               confidence, valid）
 *
 * 坐标系：x = dist * cos(angle_rad), y = dist * sin(angle_rad)
 */

#ifndef __CLUSTER_H
#define __CLUSTER_H

#include <stdint.h>
#include "radar_process.h"   /* RawPoint_t */
#include "target_detect.h"   /* TargetCandidate_t, CLUSTER_EPS_CM, CLUSTER_MIN_POINTS */

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/
void     Cluster_Init(void);
uint8_t  Cluster_DBSCAN(RawPoint_t *points, uint8_t cnt, TargetCandidate_t *targets);

#ifdef __cplusplus
}
#endif

#endif /* __CLUSTER_H */
