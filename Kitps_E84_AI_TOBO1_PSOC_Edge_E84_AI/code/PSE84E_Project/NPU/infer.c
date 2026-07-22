/**
 * @file infer.c
 * @brief NPU 推理入口 —— 全图直接推理（标准 ML 流程）
 *
 * 方法说明：
 *   使用 Edge Impulse / ModusToolbox AI 等 Infineon 官方 ML 工具链，
 *   直接在 160x120 全图分辨率上训练二分类模型（人/背景）。
 *   推理时：采集 → 全图归一化 → NPU 推理 → 读取置信度。
 *
 *   相较于手工区块细扫方案，该方案：
 *   - 让 Ethos-U55 NPU 发挥其设计能力（跑完整 CNN）
 *   - 代码量减少约 80%，调试负担大幅降低
 *   - 与 Infineon 官方 ML 工具链完全对齐
 *   - 整体识别效果由训练数据质量决定，而非手工特征质量
 *
 * 训练流程（见 docs/training_guide.md）：
 *   Step 1: 用 PSE84E OV7675 采集各场景下人/背景的 160x120 图像
 *   Step 2: 导入 Edge Impulse 或 ModusToolbox AI，标注分类
 *   Step 3: 训练并导出 int8 量化权重
 *   Step 4: 用 model_export.py 生成 model_weights.h
 *   Step 5: 编译烧录
 */

#include "infer.h"
#include "npu.h"
#include "utils/preprocess.h"
#include "utils/postprocess.h"

/**
 * @brief 运行一次 NPU 全图推理
 * @param image_data  160x120 灰度图像缓冲区（uint8, 范围 0~255）
 * @return NPU_Result_t 包含是否检测到人、置信度
 *
 * 调用流程：
 *   1. Preprocess_Run()    — uint8[0..255] → int8[-128..127] 量化映射，写入 NPU_INPUT
 *   2. NPU_Invoke()        — 启动 NPU 推理（阻塞等待完成）
 *   3. Postprocess_Run()   — 读取 NPU_OUTPUT[0]/[1]，int8→float 反量化，判断是否为人
 *
 * ⚠️ NNLite 硬件原生 INT8 量化，NPU_INPUT/NPU_OUTPUT 均为 int8_t* 类型。
 *    切勿将寄存器声明为 float*。
 *
 * ⚠️ 当前使用固定 160x120 输入。若更换模型分辨率，
 *    需同步修改 npu.h 中的 NPU_INPUT_W / NPU_INPUT_H。
 */
NPU_Result_t Infer_Run(uint8_t *image_data)
{
    // ---- 第 1 步：全图归一化并送入 NPU 输入缓冲区 ----
    Preprocess_Run(image_data, NPU_INPUT_W, NPU_INPUT_H);

    // ---- 第 2 步：启动 NPU 推理（阻塞等待完成） ----
    NPU_Invoke();

    // ---- 第 3 步：读取输出，判断是否为人体目标 ----
    NPU_Result_t result = Postprocess_Run();

    return result;
}
