/**
 * @file postprocess.c
 * @brief NPU 后处理 —— 将 int8 量化输出转为人可读的置信度
 *
 * NNLite 输出为 int8 原始值，范围[-128, 127]。
 * 值越大表示对应类别置信度越高。
 *
 * 从原始 int8 值到浮点置信度的映射方式：
 *   confidence = (score + 128) / 255.0f    → [0.0, 1.0]
 *
 * 此映射依赖于训练时的量化方案（量化参数 scale + zero_point）。
 * 如果 Edge Impulse / ModusToolbox AI 导出的模型使用不同的量化参数，
 * 需根据实际导出的 quantization details 调整此映射。
 */

#include "postprocess.h"
#include "../npu.h"

/**
 * @brief 执行 NPU 后处理
 * @return NPU_Result_t 包含是否检测到人、置信度
 *
 * NPU_OUTPUT[0] = 背景置信度（int8, 越大越像背景）
 * NPU_OUTPUT[1] = 人体置信度（int8, 越大越像人体）
 */
NPU_Result_t Postprocess_Run(void)
{
    NPU_Result_t res = {0};

    int8_t score_bg    = NPU_OUTPUT[0];
    int8_t score_human = NPU_OUTPUT[1];

    /* int8[-128,127] → float[0.0, 1.0] */
    float conf_bg    = (float)((int)score_bg + 128) / 255.0f;
    float conf_human = (float)((int)score_human + 128) / 255.0f;

    res.confidence = conf_human;
    res.is_human   = (conf_human > conf_bg);

    /* 全图推理模式下，NPU 不输出位置信息。
     * center_x/center_y 在 Infer_Run 中由调用方根据云台角度推导。 */
    res.center_x = 0;
    res.center_y = 0;

    return res;
}

/**
 * @brief 调度器调用的后处理包装函数（适配 scheduler.c 接口）
 * @param confidence   输出置信度 (0.0~1.0)
 * @param offset_angle 输出角度偏移（相对视场中心，度）
 *
 * 注意：当前全图推理方案中，offset_angle 固定为 0。
 *       定位由雷达 + 云台系统完成，NPU 仅做确认。
 */
void Postprocess_GetResult(float *confidence, float *offset_angle)
{
    NPU_Result_t res = Postprocess_Run();

    *confidence = res.confidence;
    *offset_angle = 0.0f;  /* 全图推理不产生角度偏移 */
}
