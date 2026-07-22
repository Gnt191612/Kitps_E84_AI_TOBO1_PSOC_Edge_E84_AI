/**
 * @file preprocess.c
 * @brief 图像预处理 —— uint8 灰度图 → int8 量化 → NPU 输入缓冲区
 *
 * NNLite 硬件原生 INT8 量化，输入范围 int8[-128, 127]。
 * 映射方式：uint8[0,255] → int8[-128,127]。
 *
 * 旧版本曾使用 float 归一化，现已废弃。
 */

#include "preprocess.h"
#include "../npu.h"

/**
 * @brief 将 uint8 灰度图像预处理后送入 NPU
 * @param img  输入图像（uint8, 0~255）
 * @param w    图像宽度（必须 = NPU_INPUT_W）
 * @param h    图像高度（必须 = NPU_INPUT_H）
 *
 * 处理流程：
 *   1. uint8[0..255] → int8[-128..127] 映射
 *   2. 直接写入 NPU_INPUT 缓冲区
 */
void Preprocess_Run(uint8_t *img, int w, int h)
{
    for (int i = 0; i < w * h; i++) {
        /* uint8 → int8 量化映射 */
        NPU_INPUT[i] = (int8_t)((int)img[i] - 128);
    }
}

/**
 * @brief 调度器调用的预处理包装函数（适配 scheduler.c 接口）
 * @param raw_data  输入数据缓冲区
 * @param len       数据长度
 *
 * 注意：当前 NV 方案已不再使用雷达回波数据。
 *       此函数保留仅作为调度器兼容接口，实际流程直接调用 Preprocess_Run。
 */
void Preprocess_RawData(float *raw_data, uint32_t len)
{
    /* 将 float 数据先映射到 uint8 [0,255]，再转为 int8 */
    uint8_t img_buf[NPU_INPUT_W * NPU_INPUT_H];
    uint32_t copy_len = (len < (uint32_t)(NPU_INPUT_W * NPU_INPUT_H))
                        ? len : (NPU_INPUT_W * NPU_INPUT_H);

    for (uint32_t i = 0; i < copy_len; i++) {
        float val = raw_data[i];
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;
        img_buf[i] = (uint8_t)(val * 255.0f);
    }
    for (uint32_t i = copy_len; i < (uint32_t)(NPU_INPUT_W * NPU_INPUT_H); i++) {
        img_buf[i] = 0;
    }

    /* 送入 NPU（内部完成 uint8 → int8 映射） */
    NPU_SetInput(img_buf);
}
