/**
 * @file    assist_localize.c
 * @brief   双ESP32互相辅助定位
 *
 * 当单目标被一个ESP32跟踪时，让另一个ESP32从不同角度观察同一目标，
 * 为后续三角定位校准距离和方向提供第二视角。
 *
 * 辅助方位 = 目标方位 + 偏移量，偏移量 = 1.5 × atan2(半基线, 目标距离)。
 * 辅助云台指向该方位后，目标在辅助相机中位于约 -偏移量 处。
 *
 * 三维安装布局：
 *   跟踪点-A: (-0.2m, 0, 0)
 *   跟踪点-B: (+0.2m, 0, 0)
 *   基线: 0.4m
 *
 * 注意：两路观测的三角解算尚未实现，辅助结果目前只记录、不参与融合。
 */

#include "assist_localize.h"
#include "config_mount.h"
#include <math.h>

#ifndef M_PI
#define M_PI  3.14159265358979323846f
#endif
#define RAD2DEG(r)  ((r) * (180.0f / M_PI))

void AssistLocalize_Init(void) {}

float AssistLocalize_ComputeBearing(const TrackedTarget_t *t)
{
    if (!t) return 0.0f;

    float half_base_cm = DEVICE_TRACK_BASE_DIST_CM / 2.0f;  /* 20cm */
    float dist_cm = (t->distance_cm > 20.0f) ? t->distance_cm : 20.0f;

    /* 视差角 = atan2(半基线, 距离)，安全系数 1.5 */
    float parallax_rad = atan2f(half_base_cm, dist_cm);
    float offset = RAD2DEG(parallax_rad) * 1.5f;

    /* 限幅：最小 3°，最大 45° */
    if (offset < 3.0f)  offset = 3.0f;
    if (offset > 45.0f) offset = 45.0f;

    return t->angle_deg + offset;
}
