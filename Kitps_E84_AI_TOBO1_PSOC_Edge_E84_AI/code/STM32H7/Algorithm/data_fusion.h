/**
 * @file    data_fusion.h
 * @brief   多传感器数据融合（统一平面坐标系，见 geometry.h）
 *
 * 所有观测先换算成“观测点 + 方位角 + 距离”，再转为平面坐标（cm）融合：
 *   84E    ：识别点 + 84E 云台方位 + 雷达距离（84E 本身不测距）
 *   ESP32  ：跟踪点 + 主云台方位 + 图像水平偏差 + 雷达距离
 *   三角定位：两块 ESP32 的方位射线交点（不依赖雷达距离）
 *
 * 公开接口：
 *   DataFusion_Init()
 *   DataFusion_UpdateWith84E()       - 融合84E识别结果
 *   DataFusion_UpdateWithESP32()     - 融合ESP32视觉跟踪
 *   DataFusion_UpdateTriangulated()  - 融合双ESP32三角定位结果
 *   DataFusion_Get()                 - 获取融合后的目标状态
 *   DataFusion_Remove()              - 目标释放时清除
 *   DataFusion_ESP32Bearing()        - ESP32 回传 x 换算为方位角
 */

#ifndef __DATA_FUSION_H
#define __DATA_FUSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t target_id;
    float   x, y;           /* 平面融合坐标 (cm) */
    float   confidence;
    float   vel_x, vel_y;   /* 平面速度 (cm/s) */
    uint32_t last_tick;
    uint8_t updated_by;     /* bit0=84E, bit1=ESP32, bit2=三角定位 */
} FusedTarget_t;

typedef struct {
    FusedTarget_t targets[3];
    uint8_t count;
} DataFusion_t;

#define FUSION_SRC_84E    0x01U
#define FUSION_SRC_ESP32  0x02U
#define FUSION_SRC_TRI    0x04U

void DataFusion_Init(DataFusion_t *f);
void DataFusion_UpdateWith84E(DataFusion_t *f, uint8_t id, float conf,
                              float bearing_deg, float dist_cm, uint32_t tick);
void DataFusion_UpdateWithESP32(DataFusion_t *f, uint8_t id, uint8_t esp_id,
                                float bearing_deg, float dist_cm, uint32_t tick);
void DataFusion_UpdateTriangulated(DataFusion_t *f, uint8_t id,
                                   float x_cm, float y_cm, uint32_t tick);
const FusedTarget_t *DataFusion_Get(const DataFusion_t *f, uint8_t id);
void DataFusion_Remove(DataFusion_t *f, uint8_t id);

/* ESP32 回传 x（伪 mm）+ 该板云台方位 → 目标方位角 */
float DataFusion_ESP32Bearing(float gimbal_bearing_deg, float x_pseudo_mm);

#ifdef __cplusplus
}
#endif

#endif /* __DATA_FUSION_H */
