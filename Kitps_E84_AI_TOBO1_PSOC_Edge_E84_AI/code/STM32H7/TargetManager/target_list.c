/**
 * @file    target_list.c
 * @brief   目标列表实现
 */

#include "target_list.h"
#include <string.h>
#include <math.h>

void TargetList_Init(TargetList_t *list)
{
    memset(list, 0, sizeof(TargetList_t));
}

int8_t TargetList_FindOrAdd(TargetList_t *list, TargetCandidate_t *cand)
{
    /* 简单匹配：角度差<10deg，距离差<50cm 视为同一目标 */
    for (int i = 0; i < list->count; i++) {
        if (fabsf(list->targets[i].angle_deg - cand->angle_deg) < 10.0f &&
            fabsf(list->targets[i].distance_cm - cand->distance_cm) < 50.0f) {
            list->targets[i].distance_cm = cand->distance_cm;
            list->targets[i].angle_deg = cand->angle_deg;
            return i;
        }
    }
    if (list->count < TARGET_MAX_COUNT) {
        list->targets[list->count].id = list->count + 1;
        list->targets[list->count].distance_cm = cand->distance_cm;
        list->targets[list->count].angle_deg = cand->angle_deg;
        list->targets[list->count].state = 0;
        list->count++;
        return list->count - 1;
    }
    return -1;
}

void TargetList_Remove(TargetList_t *list, uint8_t id)
{
    if (id == 0 || id > list->count) return;
    memmove(&list->targets[id-1], &list->targets[id], (list->count - id) * sizeof(TrackedTarget_t));
    list->count--;
}

TrackedTarget_t* TargetList_GetTarget(TargetList_t *list, uint8_t id)
{
    if (id > 0 && id <= list->count) return &list->targets[id-1];
    return NULL;
}