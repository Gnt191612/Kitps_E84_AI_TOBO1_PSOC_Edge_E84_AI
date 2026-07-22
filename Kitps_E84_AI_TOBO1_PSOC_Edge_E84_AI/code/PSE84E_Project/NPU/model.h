/**
 * @file model.h
 * @brief 轻量模型部署接口（int8 量化）
 *
 * 模型规格：
 *   输入：160x120 灰度图（uint8, 内部映射为 int8[-128,127]）
 *   输出：2 类（int8: 背景=0, 人体=1）
 *   权重格式：uint8_t 数组（int8 量化值，由 Edge Impulse / ModusToolbox AI 导出）
 */

#ifndef MODEL_H
#define MODEL_H

#include <stdint.h>

#define MODEL_INPUT_W       160
#define MODEL_INPUT_H       120
#define MODEL_CLASS_NUM     2

void Model_Load(void);

#endif /* MODEL_H */