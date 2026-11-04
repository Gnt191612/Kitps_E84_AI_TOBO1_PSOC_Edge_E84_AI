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
        /* id 在目标生命周期内保持不变（ESP32/84E 回传会携带它），
         * 取 1..TARGET_MAX_COUNT 中最小的未用值，不能用列表下标 */
        uint8_t new_id = 0;
        for (uint8_t cand_id = 1; cand_id <= TARGET_MAX_COUNT && new_id == 0; cand_id++) {
            if (!TargetList_GetTarget(list, cand_id)) new_id = cand_id;
        }
        TrackedTarget_t *t = &list->targets[list->count];
        memset(t, 0, sizeof(*t));
        t->id = new_id;
        t->distance_cm = cand->distance_cm;
        t->angle_deg = cand->angle_deg;
        t->esp_assigned = TARGET_ESP_NONE;
        list->count++;
        return list->count - 1;
    }
    return -1;
}

void TargetList_Remove(TargetList_t *list, uint8_t id)
{
    for (int i = 0; i < list->count; i++) {
        if (list->targets[i].id == id) {
            memmove(&list->targets[i], &list->targets[i + 1],
                    (list->count - i - 1) * sizeof(TrackedTarget_t));
            list->count--;
            return;
        }
    }
}

TrackedTarget_t* TargetList_GetTarget(TargetList_t *list, uint8_t id)
{
    for (int i = 0; i < list->count; i++) {
        if (list->targets[i].id == id) return &list->targets[i];
    }
    return NULL;
}