/**
 * @file    data_fusion.c
 * @brief   融合实现：统一平面坐标 + 按来源加权平均 + 速度估计
 */

#include "data_fusion.h"
#include "geometry.h"
#include "config_mount.h"
#include <string.h>
#include <math.h>

/* 新观测权重：三角定位不依赖雷达距离，可信度最高 */
#define W_ESP32   0.6f
#define W_TRI     0.8f

void DataFusion_Init(DataFusion_t *f)
{
    memset(f, 0, sizeof(DataFusion_t));
}

static FusedTarget_t *FindOrAdd(DataFusion_t *f, uint8_t id)
{
    for (int i = 0; i < f->count; i++) {
        if (f->targets[i].target_id == id) return &f->targets[i];
    }
    if (f->count >= 3) return NULL;
    FusedTarget_t *t = &f->targets[f->count++];
    memset(t, 0, sizeof(*t));
    t->target_id = id;
    return t;
}

/* 以权重 w 融合新平面观测，并用位移估计速度 */
static void Blend(FusedTarget_t *t, float x, float y, float w,
                  uint32_t tick, uint8_t src)
{
    if (t->updated_by == 0U) {
        t->x = x;
        t->y = y;
    } else {
        float nx = t->x * (1.0f - w) + x * w;
        float ny = t->y * (1.0f - w) + y * w;
        uint32_t dt_ms = tick - t->last_tick;
        if (dt_ms > 0U && dt_ms < 1000U) {
            float dt = (float)dt_ms / 1000.0f;
            t->vel_x = 0.7f * t->vel_x + 0.3f * (nx - t->x) / dt;
            t->vel_y = 0.7f * t->vel_y + 0.3f * (ny - t->y) / dt;
        }
        t->x = nx;
        t->y = ny;
    }
    t->last_tick = tick;
    t->updated_by |= src;
}

void DataFusion_UpdateWith84E(DataFusion_t *f, uint8_t id, float conf,
                              float bearing_deg, float dist_cm, uint32_t tick)
{
    FusedTarget_t *t = FindOrAdd(f, id);
    if (!t || dist_cm <= 0.0f) return;
    float x, y;
    Geometry_PolarToPlane(DEVICE_RECOG_X_CM, DEVICE_RECOG_Y_CM,
                          bearing_deg, dist_cm, &x, &y);
    if (conf < 0.0f) conf = 0.0f;
    if (conf > 1.0f) conf = 1.0f;
    Blend(t, x, y, conf, tick, FUSION_SRC_84E);
    t->confidence = conf;
}

void DataFusion_UpdateWithESP32(DataFusion_t *f, uint8_t id, uint8_t esp_id,
                                float bearing_deg, float dist_cm, uint32_t tick)
{
    FusedTarget_t *t = FindOrAdd(f, id);
    if (!t || dist_cm <= 0.0f || esp_id > 1U) return;
    float ox = (esp_id == 0U) ? DEVICE_TRACK_A_X_CM : DEVICE_TRACK_B_X_CM;
    float oy = (esp_id == 0U) ? DEVICE_TRACK_A_Y_CM : DEVICE_TRACK_B_Y_CM;
    float x, y;
    Geometry_PolarToPlane(ox, oy, bearing_deg, dist_cm, &x, &y);
    Blend(t, x, y, W_ESP32, tick, FUSION_SRC_ESP32);
}

void DataFusion_UpdateTriangulated(DataFusion_t *f, uint8_t id,
                                   float x_cm, float y_cm, uint32_t tick)
{
    FusedTarget_t *t = FindOrAdd(f, id);
    if (!t) return;
    Blend(t, x_cm, y_cm, W_TRI, tick, FUSION_SRC_TRI);
}

const FusedTarget_t *DataFusion_Get(const DataFusion_t *f, uint8_t id)
{
    for (int i = 0; i < f->count; i++) {
        if (f->targets[i].target_id == id) return &f->targets[i];
    }
    return NULL;
}

void DataFusion_Remove(DataFusion_t *f, uint8_t id)
{
    for (int i = 0; i < f->count; i++) {
        if (f->targets[i].target_id == id) {
            memmove(&f->targets[i], &f->targets[i + 1],
                    (f->count - i - 1) * sizeof(FusedTarget_t));
            f->count--;
            return;
        }
    }
}

float DataFusion_ESP32Bearing(float gimbal_bearing_deg, float x_pseudo_mm)
{
    float px = x_pseudo_mm / ESP32_PIXEL_TO_MM_X;
    float px_per_deg = ESP32_IMG_WIDTH_PX / ESP32_CAM_FOV_H_DEG;
    float offset_deg = (px - ESP32_IMG_WIDTH_PX / 2.0f) / px_per_deg;
    return gimbal_bearing_deg + ESP32_IMG_X_BEARING_SIGN * offset_deg;
}
