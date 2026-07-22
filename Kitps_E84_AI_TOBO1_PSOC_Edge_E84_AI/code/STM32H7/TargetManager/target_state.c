/**
 * @file    target_state.c
 * @brief   状态更新逻辑
 */

#include "target_state.h"

void TargetState_Update(TargetList_t *list, uint8_t id, uint8_t new_state)
{
    TrackedTarget_t *t = TargetList_GetTarget(list, id);
    if (t) {
        t->state = new_state;
    }
}