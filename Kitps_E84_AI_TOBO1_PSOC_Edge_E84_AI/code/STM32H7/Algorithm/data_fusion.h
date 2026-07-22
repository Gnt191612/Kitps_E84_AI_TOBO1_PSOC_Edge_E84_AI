/**
 * @file    data_fusion.h
 * @brief   多传感器数据融合
 *
 * 公开接口：
 *   DataFusion_Init()
 *   DataFusion_UpdateWith84E()  - 融合84E识别结果
 *   DataFusion_UpdateWithESP32() - 融合ESP32视觉跟踪
 *   DataFusion_GetState()        - 获取融合后的目标状态
 */

#ifndef __DATA_FUSION_H
#define __DATA_FUSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t target_id;
    float   x, y;           /* 基平面 (XY 平面) 上的融合坐标 */
    float   confidence;
    float   vel_x, vel_y;   /* XY 平面上的速度分量 */
    uint8_t updated_by;     /* 1=84E, 2=ESP32, 3=both */
} FusedTarget_t;

typedef struct {
    FusedTarget_t targets[3];
    uint8_t count;
} DataFusion_t;

void DataFusion_Init(DataFusion_t *f);
void DataFusion_UpdateWith84E(DataFusion_t *f, uint8_t id, float conf, float x, float y, float dist, float angle);
void DataFusion_UpdateWithESP32(DataFusion_t *f, uint8_t id, float x, float y, uint8_t lost);
void DataFusion_On84EResult(uint8_t *payload, uint16_t len);   // 回调格式
void DataFusion_OnESP32Track(uint8_t *payload, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __DATA_FUSION_H */