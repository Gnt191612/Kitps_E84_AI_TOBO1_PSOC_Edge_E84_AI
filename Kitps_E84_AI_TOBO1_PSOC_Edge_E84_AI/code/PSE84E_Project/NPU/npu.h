/**
 * @file    npu.h
 * @brief   NPU 寄存器映射与驱动 API
 *
 * KIT_PSE84_AI（PSE846GPS2DBZC4A）集成 Ethos-U55 NPU：
 *
 *   1. NNLite（低功耗常启）—— 基地址 0x40080000
 *      挂载在 APB 总线，由 M33 核调用。
 *      有片上 INPUT/OUTPUT 寄存器，适合轻量推理。
 *      当前项目使用此 NPU。
 *
 *   2. Ethos-U55（高性能主 NPU）—— 基地址 0x42500000
 *      挂载在高性能总线，由 M55 核调用。
 *      无内置 INPUT/OUTPUT 寄存器（数据通过专用内存接口传递）。
 *      如需使用，需通过 M55 + TFLite Micro 后端驱动。
 *
 * ⚠️ 输入输出数据类型为 int8（硬件原生仅支持 INT8 量化）。
 *    禁止使用 float* 声明寄存器指针。
 */

#ifndef __NPU_H
#define __NPU_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/postprocess.h"

/* ======================================================================== */
/* NNLite — 当前使用的低功耗 NPU                                             */
/* ======================================================================== */
#define NNLITE_BASE         0x40080000UL

#define NPU_CTRL            (*(volatile uint32_t *)(NNLITE_BASE + 0x000))
#define NPU_STATUS          (*(volatile uint32_t *)(NNLITE_BASE + 0x004))
#define NPU_INPUT           ((volatile int8_t *)(NNLITE_BASE + 0x100))
#define NPU_OUTPUT          ((volatile int8_t *)(NNLITE_BASE + 0x800))

/* 模型固定输入尺寸（160x120 灰度图，int8 量化） */
#define NPU_INPUT_W         160
#define NPU_INPUT_H         120
#define NPU_OUTPUT_CLASSES  2

/* CTRL 控制位（来自 NNLite 寄存器手册确认） */
#define NPU_CTRL_START      (1UL << 0)   /* 启动推理 */
#define NPU_CTRL_RESET      (1UL << 1)   /* 软复位 */
#define NPU_CTRL_SOFT_RST   NPU_CTRL_RESET /* 别名 */

/* STATUS 状态位（来自 NNLite 寄存器手册确认） */
#define NPU_STATUS_IDLE     (0UL)
#define NPU_STATUS_BUSY     (1UL << 0)
#define NPU_STATUS_DONE     (1UL << 1)
#define NPU_STATUS_ERROR    (1UL << 2)

/* 权重加载区硬件上限 256KB (0x40000) */
#define NPU_WEIGHT_MAX_SIZE  0x40000UL

/* 图像尺寸宏（与 NPU 输入一致） */
#ifndef IMG_W
#define IMG_W   NPU_INPUT_W
#endif
#ifndef IMG_H
#define IMG_H   NPU_INPUT_H
#endif

/* ======================================================================== */
/* Ethos-U55（预留，当前未使用）                                             */
/* ======================================================================== */
#define ETHOS_U55_BASE      0x42500000UL

/* ======================================================================== */
/* NPU 状态枚举                                                              */
/* ======================================================================== */
typedef enum {
    NPU_STATE_IDLE,
    NPU_STATE_BUSY,
    NPU_STATE_DONE,
    NPU_STATE_ERROR
} NPU_State_t;

/* ======================================================================== */
/* NPU 驱动 API                                                              */
/* ======================================================================== */

/* 初始化硬件 */
void NPU_Init(void);

/* 加载模型权重（权重数据为 int8 量化值） */
void NPU_LoadWeights(const uint8_t *weights);

/* 送入图像数据（uint8 灰度图，内部自动映射到 int8 量化范围） */
void NPU_SetInput(uint8_t *img_buf);

/* 启动一次推理（阻塞等待完成） */
void NPU_Invoke(void);

/* 获取当前状态 */
NPU_State_t NPU_GetState(void);

/* 获取 NPU 原始输出（索引 0=背景, 1=人体，返回 int8 量化值） */
int8_t NPU_GetOutputScore(int index);

/* 调度器包装函数：NPU 模型初始化 */
int  NPU_Model_Init(void);

/* 调度器包装函数：运行 NPU 推理 */
void NPU_RunInference(void);

/* model.c 调用的 NPU 输入尺寸设置 */
void NPU_SetInputSize(uint32_t width, uint32_t height);

#endif /* __NPU_H */
