/**
 * @file    assist_localize.c
 * @brief   双ESP32互相辅助定位
 *
 * 当单目标被一个ESP32跟踪时，让另一个ESP32从不同角度观察同一目标，
 * 为后续三角定位校准距离和方向提供第二视角。
 *
 * 辅助方位：由主板方位和目标距离算出目标的平面位置，再换算到辅助板自己的位置。
 * 两块板相距 40 cm，人越近方位差越大，1.3 m 处约差 20°，不能直接用主板的方位。
 *
 * 三维安装布局：
 *   跟踪点-A: (-0.2m, 0, 0)
 *   跟踪点-B: (+0.2m, 0, 0)
 *   基线: 0.4m
 */

#include "assist_localize.h"
#include "config_mount.h"
#include "geometry.h"

void AssistLocalize_Init(void) {}

float AssistLocalize_ComputeBearing(const TrackedTarget_t *t)
{
    if (!t) return 0.0f;

    float dist_cm = (t->distance_cm > 20.0f) ? t->distance_cm : 20.0f;
    uint8_t primary_is_a = (t->esp_assigned == 0U);
    float x, y, bearing, dist;
    Geometry_PolarToPlane(primary_is_a ? DEVICE_TRACK_A_X_CM : DEVICE_TRACK_B_X_CM,
                          primary_is_a ? DEVICE_TRACK_A_Y_CM : DEVICE_TRACK_B_Y_CM,
                          t->angle_deg, dist_cm, &x, &y);
    Geometry_PlaneToPolar(primary_is_a ? DEVICE_TRACK_B_X_CM : DEVICE_TRACK_A_X_CM,
                          primary_is_a ? DEVICE_TRACK_B_Y_CM : DEVICE_TRACK_A_Y_CM,
                          x, y, &bearing, &dist);
    return bearing;
}
