#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdint.h>

void Preprocess_Run(uint8_t *img, int w, int h);

/**
 * @brief 调度器调用的预处理包装函数（雷达回波数据 → NPU 输入）
 * @param raw_data  雷达回波强度数据
 * @param len       数据长度
 */
void Preprocess_RawData(float *raw_data, uint32_t len);

#endif