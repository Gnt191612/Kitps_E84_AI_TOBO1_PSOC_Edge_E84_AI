/**
 * @file    data_fusion.c
 * @brief   融合实现：加权平均+卡尔曼更新
 */

#include "data_fusion.h"
#include "kalman_filter.h"
#include <string.h>
#include <math.h>

void DataFusion_Init(DataFusion_t *f)
{
    memset(f, 0, sizeof(DataFusion_t));
}

void DataFusion_UpdateWith84E(DataFusion_t *f, uint8_t id, float conf, float x, float y, float dist, float angle)
{
    for (int i = 0; i < f->count; i++) {
        if (f->targets[i].target_id == id) {
            f->targets[i].x = x * conf + f->targets[i].x * (1 - conf);
            f->targets[i].y = y * conf + f->targets[i].y * (1 - conf);
            f->targets[i].confidence = conf;
            f->targets[i].updated_by = 1;  // 84E
            return;
        }
    }
    if (f->count < 3) {
        f->targets[f->count].target_id = id;
        f->targets[f->count].x = x;
        f->targets[f->count].y = y;
        f->targets[f->count].confidence = conf;
        f->targets[f->count].updated_by = 1;
        f->count++;
    }
}

void DataFusion_UpdateWithESP32(DataFusion_t *f, uint8_t id, float x, float y, uint8_t lost)
{
    for (int i = 0; i < f->count; i++) {
        if (f->targets[i].target_id == id) {
            if (!lost) {
                f->targets[i].x = f->targets[i].x * 0.3f + x * 0.7f;
                f->targets[i].y = f->targets[i].y * 0.3f + y * 0.7f;
            }
            f->targets[i].updated_by |= 0x02;  // ESP32 updated
            return;
        }
    }
}

/* 协议回调适配 */
void DataFusion_On84EResult(uint8_t *payload, uint16_t len)
{
    /* 按协议解析，这里简化调用上层函数，实际需反序列化 */
}

void DataFusion_OnESP32Track(uint8_t *payload, uint16_t len)
{
    /* 同上 */
}