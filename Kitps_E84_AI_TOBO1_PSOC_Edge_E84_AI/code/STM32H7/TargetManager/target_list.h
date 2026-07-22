/**
 * @file    target_list.h
 * @brief   多目标列表管理
 *
 * 公开接口：
 *   TargetList_Init()
 *   TargetList_FindOrAdd() - 匹配已有目标或新增
 *   TargetList_Remove()
 *   TargetList_GetTarget()
 */

#ifndef __TARGET_LIST_H
#define __TARGET_LIST_H

#include <stdint.h>
#include "target_detect.h"   // TargetCandidate_t

#define TARGET_MAX_COUNT 3

typedef struct {
    uint8_t id;
    float   distance_cm;
    float   angle_deg;
    float   x, y;        /* 基平面 (XY 平面) 上的投影坐标 */
    uint8_t state;       // 0:空闲 1:扫描 2:跟踪 3:丢失
    uint8_t esp_assigned; // 分配到的ESP32编号(0/1)
} TrackedTarget_t;

typedef struct {
    TrackedTarget_t targets[TARGET_MAX_COUNT];
    uint8_t count;
} TargetList_t;

void  TargetList_Init(TargetList_t *list);
int8_t TargetList_FindOrAdd(TargetList_t *list, TargetCandidate_t *cand);
void  TargetList_Remove(TargetList_t *list, uint8_t id);
TrackedTarget_t* TargetList_GetTarget(TargetList_t *list, uint8_t id);

#endif /* __TARGET_LIST_H */