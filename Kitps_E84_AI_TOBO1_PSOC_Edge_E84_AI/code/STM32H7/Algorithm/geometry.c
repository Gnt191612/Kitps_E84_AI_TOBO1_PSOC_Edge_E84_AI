/**
 * @file    geometry.c
 * @brief   统一平面坐标系与双视角三角定位实现
 */

#include "geometry.h"
#include <math.h>

#ifndef M_PI
#define M_PI  3.14159265358979323846f
#endif
#define DEG2RAD(d)  ((d) * (M_PI / 180.0f))
#define RAD2DEG(r)  ((r) * (180.0f / M_PI))

/* 两射线夹角正弦小于该值视为近似平行（约 3°），交点误差过大 */
#define TRIANGULATE_MIN_SIN   0.05f

void Geometry_PolarToPlane(float ox_cm, float oy_cm,
                           float bearing_deg, float dist_cm,
                           float *x_cm, float *y_cm)
{
    float rad = DEG2RAD(bearing_deg);
    *x_cm = ox_cm + dist_cm * sinf(rad);
    *y_cm = oy_cm + dist_cm * cosf(rad);
}

void Geometry_PlaneToPolar(float ox_cm, float oy_cm,
                           float x_cm, float y_cm,
                           float *bearing_deg, float *dist_cm)
{
    float dx = x_cm - ox_cm;
    float dy = y_cm - oy_cm;
    *bearing_deg = RAD2DEG(atan2f(dx, dy));
    *dist_cm = sqrtf(dx * dx + dy * dy);
}

uint8_t Geometry_Triangulate(float ax_cm, float ay_cm, float bearing_a_deg,
                             float bx_cm, float by_cm, float bearing_b_deg,
                             float *x_cm, float *y_cm)
{
    /* A + s·dA = B + u·dB，d = (sin θ, cos θ) */
    float dax = sinf(DEG2RAD(bearing_a_deg)), day = cosf(DEG2RAD(bearing_a_deg));
    float dbx = sinf(DEG2RAD(bearing_b_deg)), dby = cosf(DEG2RAD(bearing_b_deg));

    float cross = dax * dby - day * dbx;   /* = sin(θb - θa) */
    if (fabsf(cross) < TRIANGULATE_MIN_SIN) return 0U;

    float wx = bx_cm - ax_cm;
    float wy = by_cm - ay_cm;
    float s = (wx * dby - wy * dbx) / cross;
    float u = (wx * day - wy * dax) / cross;
    if (s <= 0.0f || u <= 0.0f) return 0U;   /* 交点在某个相机背后 */

    *x_cm = ax_cm + s * dax;
    *y_cm = ay_cm + s * day;
    return 1U;
}
