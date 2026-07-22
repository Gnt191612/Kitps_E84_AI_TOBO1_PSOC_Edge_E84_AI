/**
 * @file    npu.c
 * @brief   NNLite NPU 寄存器级驱动实现
 *
 * 输入/输出数据类型：int8（硬件原生 INT8 量化）。
 * 输入图像在送入前已完成 uint8→int8 映射（Preprocess_Run 中处理）。
 */

#include "npu.h"
#include "model.h"
#include "model/model_weights.h"

/* NPU 权重缓冲区位置（硬件上限 256KB = 0x40000） */
#define NPU_WEIGHT          ((volatile uint8_t *)(NNLITE_BASE + 0x1000))
#define NPU_WEIGHT_SIZE_MAX 0x40000UL

static NPU_State_t npu_state = NPU_STATE_IDLE;

/* ======================================================================== */
/* NPU 初始化                                                               */
/* ======================================================================== */
void NPU_Init(void)
{
    /* 复位 NPU */
    NPU_CTRL |= NPU_CTRL_RESET;
    NPU_CTRL &= ~NPU_CTRL_RESET;
    npu_state = NPU_STATE_IDLE;
}

/* ======================================================================== */
/* 加载训练好的模型权重（int8 量化权重）                                     */
/* ======================================================================== */
void NPU_LoadWeights(const uint8_t *weights)
{
    if (WEIGHT_SIZE > NPU_WEIGHT_SIZE_MAX) {
        /* 权重超硬件上限，截断防止溢出 */
        for (uint32_t i = 0; i < NPU_WEIGHT_SIZE_MAX; i++)
            NPU_WEIGHT[i] = weights[i];
        return;
    }
    for (uint32_t i = 0; i < WEIGHT_SIZE; i++)
    {
        NPU_WEIGHT[i] = weights[i];
    }
}

/* ======================================================================== */
/* 将 uint8 灰度图像映射到 int8 量化范围并送入 NPU 输入缓冲区                */
/*                                                                           */
/* 映射方式：                                                                */
/*   uint8[0..255] → int8[-128..127]                                         */
/*   公式： int8_value = (int8_t)(img_value - 128)                           */
/*                                                                           */
/* 这是与模型训练时相同的量化方式。如果训练时使用不同的量化方案               */
/* （如 uint8→[-1,1] 浮点），请根据实际模型调整本函数。                       */
/* ======================================================================== */
void NPU_SetInput(uint8_t *img_buf)
{
    for (int i = 0; i < NPU_INPUT_W * NPU_INPUT_H; i++)
    {
        /* uint8 [0,255] → int8 [-128,127] */
        NPU_INPUT[i] = (int8_t)((int)img_buf[i] - 128);
    }
}

/* ======================================================================== */
/* 启动 NPU 推理（阻塞等待完成）                                             */
/* ======================================================================== */
void NPU_Invoke(void)
{
    npu_state = NPU_STATE_BUSY;
    NPU_CTRL |= NPU_CTRL_START;

    /* 轮询等待状态寄存器 */
    while (!(NPU_STATUS & NPU_STATUS_DONE))
    {
        if (NPU_STATUS & NPU_STATUS_ERROR)
        {
            npu_state = NPU_STATE_ERROR;
            return;
        }
        /* 可插入 WFI 指令降低功耗，但 84E 跑裸机 */
    }

    npu_state = NPU_STATE_DONE;
}

/* ======================================================================== */
/* 获取 NPU 状态                                                             */
/* ======================================================================== */
NPU_State_t NPU_GetState(void)
{
    return npu_state;
}

/* ======================================================================== */
/* 获取 NPU 原始输出（int8 量化值）                                         */
/* 索引 0 = 背景置信度，索引 1 = 人体置信度                                  */
/* 返回 int8：值越大表示对应类别的概率越高                                   */
/* ======================================================================== */
int8_t NPU_GetOutputScore(int index)
{
    if (index >= 0 && index < NPU_OUTPUT_CLASSES)
    {
        return NPU_OUTPUT[index];
    }
    return -128;  /* 无效索引返回最低值 */
}

/* ======================================================================== */
/* 调度器包装函数                                                            */
/* ======================================================================== */

int NPU_Model_Init(void)
{
    Model_Load();
    return 0;
}

void NPU_RunInference(void)
{
    NPU_Invoke();
}

void NPU_SetInputSize(uint32_t width, uint32_t height)
{
    (void)width;
    (void)height;
    /* 输入尺寸在 npu.h 中固定: NPU_INPUT_W/NPU_INPUT_H */
}
