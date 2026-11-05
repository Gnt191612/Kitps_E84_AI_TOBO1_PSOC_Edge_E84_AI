#include "closed_loop.h"
#include "target_state.h"
void ClosedLoop_Init(void) {}
uint8_t ClosedLoop_NeedsResend(const TrackedTarget_t *t,
                               uint32_t since_rx_ms, uint32_t since_resend_ms) {
    if (!t || t->esp_assigned > 1U) return 0U;
    /* 丢失中的目标也要看门狗：复锁靠主板的丢失帧推进，主板不回传就会一直卡住 */
    if (t->state != TARGET_STATE_TRACKED && t->state != TARGET_STATE_LOST) return 0U;
    /* 链路正常时绝不重发，避免ESP32反复复锁 */
    if (since_rx_ms < CLOSED_LOOP_LINK_TIMEOUT_MS) return 0U;
    return (since_resend_ms >= CLOSED_LOOP_RESEND_MIN_MS) ? 1U : 0U;
}
