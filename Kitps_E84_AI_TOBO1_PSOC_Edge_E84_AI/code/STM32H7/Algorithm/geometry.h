/**
 * @file    geometry.h
 * @brief   统一平面坐标系与双视角三角定位
 *
 * 平面坐标系（与 config_mount.h 一致，单位 cm）：
 *   原点 = 中心点；+X 沿 ESP32-A → ESP32-B；+Y 正前方。
 * 方位角（bearing，°）：0 = 正前方(+Y)，顺时针为正（偏向 +X）。
 * 雷达舵机角 = 135 + bearing；云台 Pan = 135 + bearing - 安装偏置。
 */
#ifndef __GEOMETRY_H
#define __GEOMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 从 (ox, oy) 出发、沿 bearing 方向距离 dist 处的平面坐标 */
void Geometry_PolarToPlane(float ox_cm, float oy_cm,
                           float bearing_deg, float dist_cm,
                           float *x_cm, float *y_cm);

/* 平面点相对 (ox, oy) 的方位角与距离 */
void Geometry_PlaneToPolar(float ox_cm, float oy_cm,
                           float x_cm, float y_cm,
                           float *bearing_deg, float *dist_cm);

/* 两条射线求交：A 点沿 bearing_a、B 点沿 bearing_b。
 * 返回 1 = 交点有效（两射线前方相交且不近似平行），0 = 无效。 */
uint8_t Geometry_Triangulate(float ax_cm, float ay_cm, float bearing_a_deg,
                             float bx_cm, float by_cm, float bearing_b_deg,
                             float *x_cm, float *y_cm);

#ifdef __cplusplus
}
#endif

#endif /* __GEOMETRY_H */
