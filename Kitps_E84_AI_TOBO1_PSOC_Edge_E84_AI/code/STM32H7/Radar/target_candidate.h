/**
 * @file    target_candidate.h
 * @brief   雷达候选目标（radar_map.c 输出，调度器和目标列表使用）
 */

#ifndef __TARGET_CANDIDATE_H
#define __TARGET_CANDIDATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TargetCandidate_s {
    uint8_t  id;            /* 临时ID(1~3) */
    float    x_cm;          /* X坐标(cm) */
    float    y_cm;          /* Y坐标(cm) */
    float    distance_cm;   /* 径向距离(cm) */
    float    angle_deg;     /* 方位角(°) */
    float    confidence;    /* 置信度(0~1) */
    uint8_t  valid;         /* 1:有效 */
} TargetCandidate_t;

#ifdef __cplusplus
}
#endif

#endif /* __TARGET_CANDIDATE_H */
