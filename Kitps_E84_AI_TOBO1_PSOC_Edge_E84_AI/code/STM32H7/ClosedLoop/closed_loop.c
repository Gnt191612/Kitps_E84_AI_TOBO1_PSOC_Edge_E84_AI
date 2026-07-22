#include "closed_loop.h"
#include "cmd_esp32.h"
#include "target_state.h"
void ClosedLoop_Init(void) {}
void ClosedLoop_Check(TrackedTarget_t *t) {
    if (!t || t->state != TARGET_STATE_TRACKED) return;
    /* 维持跟踪命令 */
    Cmd_ESP32_SendTrackCmd(t->esp_assigned, t->angle_deg, t->distance_cm, t->id);
}