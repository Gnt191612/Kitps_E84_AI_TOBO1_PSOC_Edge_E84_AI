#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool is_human;
    float confidence;
    int center_x;
    int center_y;
} NPU_Result_t;

NPU_Result_t Postprocess_Run(void);

/**
 * @brief 调度器调用的后处理包装函数
 * @param confidence  输出置信度
 * @param offset_angle 输出角度偏差（度）
 */
void Postprocess_GetResult(float *confidence, float *offset_angle);

#endif