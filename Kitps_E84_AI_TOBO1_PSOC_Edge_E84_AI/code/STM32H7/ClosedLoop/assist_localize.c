/**
 * @file    assist_localize.c
 * @brief   双ESP32互相辅助定位
 *
 * 当单目标被一个ESP32跟踪时，让另一个ESP32从不同角度观察同一目标，
 * 利用三角定位校准距离和方向。
 *
 * 两种辅助模式：
 *   1. 正常模式 (assist_direct_mode=0)：辅助 ESP32 以偏移量观察目标，
 *      偏移量 = 1.5 × atan2(半基线, 目标距离)，用于三角定位。
 *   2. 直接模式 (assist_direct_mode=1)：主力 ESP32 接近/超过 Pan 限位，
 *      辅助 ESP32 直接对准目标跟踪。当主力恢复范围后回到正常模式。
 *
 * 三维安装布局：
 *   跟踪点-A: (-0.2m, 0, 0)
 *   跟踪点-B: (+0.2m, 0, 0)
 *   基线: 0.4m
 */

#include "assist_localize.h"
#include "cmd_esp32.h"
#include "config_mount.h"
#include <math.h>

#ifndef M_PI
#define M_PI  3.14159265358979323846f
#endif
#define DEG2RAD(d)  ((d) * (M_PI / 180.0f))
#define RAD2DEG(r)  ((r) * (180.0f / M_PI))

/* 外部标志：由 scheduler.c 的 TryHandoverTrack 控制 */
extern uint8_t g_assist_direct_mode;

void AssistLocalize_Init(void) {}

void AssistLocalize_Execute(TrackedTarget_t *t)
{
    if (!t) return;

    uint8_t other_esp = 1 - t->esp_assigned;

    if (g_assist_direct_mode) {
        /* ── 直接跟踪模式：主力越界，辅助直接对准目标 ── */
        Cmd_ESP32_SendTrackCmd(other_esp, t->angle_deg, t->distance_cm, t->id);
        return;
    }

    /* ── 正常模式：辅助带偏移量跟踪，用于三角定位 ── */
    float half_base_cm = DEVICE_TRACK_BASE_DIST_CM / 2.0f;  /* 20cm */
    float dist_cm = (t->distance_cm > 20.0f) ? t->distance_cm : 20.0f;

    /* 视差角 = atan2(半基线, 距离)，安全系数 1.5 */
    float parallax_rad = atan2f(half_base_cm, dist_cm);
    float offset = RAD2DEG(parallax_rad) * 1.5f;

    /* 限幅：最小 3°，最大 45° */
    if (offset < 3.0f)  offset = 3.0f;
    if (offset > 45.0f) offset = 45.0f;

    float assist_angle = t->angle_deg + offset;

    Cmd_ESP32_SendTrackCmd(other_esp, assist_angle, t->distance_cm, t->id);
}
