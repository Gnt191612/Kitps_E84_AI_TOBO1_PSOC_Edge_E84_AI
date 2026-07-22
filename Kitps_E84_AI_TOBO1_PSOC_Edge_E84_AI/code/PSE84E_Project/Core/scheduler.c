/**
 * @file    scheduler.c
 * @brief   调度器实现：进程切换、图像采集、NPU 全图推理、结果上报
 *
 * 架构：
 *   84E 被动接收 H7 指令 → 采集 OV7675 图像 → 全图 NPU 推理 → 结果回传 H7
 *
 * 依赖：
 *   - protocol.h     (命令/结果包定义，Protocol_SendResult)
 *   - ov7675.h       (摄像头图像采集)
 *   - npu.h / infer.h / preprocess.h / postprocess.h
 *   - scan_control.h / roi_optimize.h (预留，辅助扫描用)
 *   - target_predict.h / kalman_filter.h
 *   - logger.h / performance.h / error_rate.h
 *
 * 注意：
 *   HC-SR04 雷达和平台舵机完全由 STM32H7 直控，PSE84E 不参与。
 *   84E 只需在收到识别指令后拍照、推理、回传。
 *   J5 GPIO 中断线可扩展用于 H7 通知事件（当前设计中 H7 通过 UART 下发指令，
 *   J5 中断作为兼容保留，见 system.c）。
 */

#include "scheduler.h"
#include "main.h"                      // HAL_GetTick
#include "communication.h"             // 串口收发
#include "protocol.h"                  // 协议收发接口
#include "ov7675.h"     // OV7675 摄像头驱动
#include "scan_control.h"
#include "roi_optimize.h"
#include "target_predict.h"
#include "kalman_filter.h"
#include "npu.h"
#include "infer.h"
#include "utils/preprocess.h"
#include "utils/postprocess.h"
#include "logger.h"
#include "performance.h"
#include "error_rate.h"

#include <string.h>
#include <math.h>

/* HAL_GetTick — 由 system.c 实现 */
extern uint32_t HAL_GetTick(void);

/* 图像缓冲区（与 OV7675 分辨率一致） */
#define IMG_BUF_SIZE    (OV7675_WIDTH * OV7675_HEIGHT)
static uint8_t g_image[IMG_BUF_SIZE];

/*----------------------------------------------------------------------------
 * 内部全局变量
 *----------------------------------------------------------------------------*/
static Process_t       g_current_process = PROC_IDLE;
static CommandPacket_t g_pending_cmd;
static uint8_t         g_cmd_ready = 0;

static TargetPredict_t g_predict;
static ScanControl_t   g_scanner;
static Kalman2D_t      g_kalman;

/*----------------------------------------------------------------------------
 * H7 命令到达回调（由 protocol.c 的帧解析调用）
 *----------------------------------------------------------------------------*/
void Scheduler_PushCommand(CommandPacket_t *cmd)
{
    g_pending_cmd = *cmd;
    g_cmd_ready = 1;
    Logger_Print(LOG_DEBUG, "Cmd rcvd: type=%d ang=%.1f wid=%.1f",
                 cmd->cmd_type, cmd->center_angle, cmd->angle_width);
}

/*----------------------------------------------------------------------------
 * J5 GPIO 中断回调（当前未使用，UART 已取代 J5 中断通知）
 *----------------------------------------------------------------------------*/
void Scheduler_Int0Callback(void) {}
void Scheduler_Int1Callback(void) {}
void Scheduler_Int2Callback(void) {}
void Scheduler_Int3Callback(void) {}

/*----------------------------------------------------------------------------
 * 调度器初始化
 *----------------------------------------------------------------------------*/
void Scheduler_Init(void)
{
    Logger_Print(LOG_INFO, "Scheduler Init...");

    /* 算法模块初始化（卡尔曼、目标预测、扫描控制） */
    TargetPredict_Init(&g_predict);
    ScanControl_Init(&g_scanner, 1.0f, -45.0f, 45.0f);
    ROI_Optimize_Init(-45.0f, 45.0f);
    Kalman2D_Init(&g_kalman, 0.05f, 0.1f, 0.1f, 5.0f, 2.0f);

    /* 通信协议初始化（注册回调） */
    Protocol_Init();
    Protocol_RegisterCommandCallback(Scheduler_PushCommand);

    /* NPU 模型加载 */
    if (NPU_Model_Init() != 0) {
        Logger_Print(LOG_ERROR, "NPU model init failed!");
    }

    /* 清空图像缓冲区 */
    memset(g_image, 0, IMG_BUF_SIZE);

    g_current_process = PROC_IDLE;
}

/*----------------------------------------------------------------------------
 * 图像采集包装函数
 *
 * 调用 OV7675_CaptureFrame() 采集一帧 160x120 灰度图像。
 * 如果摄像头驱动不可用，返回 -1 跳过本次推理循环。
 *
 * OV7675_Init() 在 system.c 的 System_Init 中已调用。
 * 如果初始化失败，可在此处尝试重新初始化。
 *----------------------------------------------------------------------------*/
static int capture_frame(void)
{
    /* 从 PSE84E 的 OV7675 摄像头采集一帧 */
    int ret = OV7675_CaptureFrame(g_image);

    if (ret != 0) {
        /* 采集失败（时序超时/线缆松动），返回错误让调度器跳过推理 */
        Logger_Print(LOG_WARN, "Camera capture failed: %d", ret);
        memset(g_image, 0, IMG_BUF_SIZE);
        return -1;
    }

    /* 成功采集到一帧 */
    return 0;
}

/*----------------------------------------------------------------------------
 * 主调度循环
 *----------------------------------------------------------------------------*/
void Scheduler_Run(void)
{
    switch (g_current_process) {

    case PROC_IDLE:
        /* 检查是否有来自 H7 的指令等待处理 */
        if (g_cmd_ready) {
            g_cmd_ready = 0;
            if (g_pending_cmd.cmd_type == 1) {
                g_current_process = PROC_AUX_SCAN;
            } else {
                g_current_process = PROC_TARGET_RECOG;
            }
        }
        break;

    /* ====================================================================
     * PROC_TARGET_RECOG — NPU 全图人体识别
     *
     * 流程：
     *   1. 采集 OV7675 摄像头图像
     *   2. 全图归一化并送入 NPU
     *   3. NPU 推理（二分类：背景/人体）
     *   4. 读取置信度，结合卡尔曼滤波平滑
     *   5. 结果回传 H7
     * ====================================================================
     */
    case PROC_TARGET_RECOG:
        {
            /* ---- Step 1: 采集图像 ---- */
            int cam_ok = capture_frame();

            ResultPacket_t res;
            memset(&res, 0, sizeof(res));
            res.target_id = 1;

            if (cam_ok == 0) {
                /* ---- Step 2: 运行全图 NPU 推理 ---- */
                Perf_StartTimer();

                NPU_Result_t npu_result = Infer_Run(g_image);

                uint32_t elapsed;
                Perf_StopTimer(&elapsed);
                Perf_RecordNPUInference(elapsed);

                /* ---- Step 3: 处理结果 ---- */
                CommandPacket_t *cmd = &g_pending_cmd;
                float center_angle = cmd->center_angle;
                float dist_cm = (cmd->min_dist_cm + cmd->max_dist_cm) / 2.0f;
                float offset_deg = 0.0f; /* 全图推理不输出偏移，雷达已定位 */

                float final_angle = center_angle + offset_deg;

                /* 卡尔曼滤波平滑 */
                float flt_dist, flt_angle, vel, ang_vel;
                Kalman2D_Update(&g_kalman, dist_cm, final_angle,
                                &flt_dist, &flt_angle, &vel, &ang_vel);

                uint32_t now = HAL_GetTick();
                TargetPredict_UpdateHistory(&g_predict, flt_dist, flt_angle, now);

                uint8_t is_human = (npu_result.confidence > 0.0f) ? 1 : 0;
                res.confidence = npu_result.confidence;
                res.angle_deg  = flt_angle;
                res.dist_cm    = flt_dist;
                float rad = flt_angle * 3.14159f / 180.0f;
                res.x_cm       = flt_dist * cosf(rad);
                res.y_cm       = flt_dist * sinf(rad);

                ErrorRate_RecordRecognition(is_human);

                Logger_Print(LOG_INFO,
                    "NPU result: %s conf=%.2f dist=%.0fcm angle=%.0f°",
                    is_human ? "HUMAN" : "BACKGROUND",
                    res.confidence, res.dist_cm, res.angle_deg);
            } else {
                /* 摄像头不可用，返回空结果 */
                res.confidence = 0.0f;
                ErrorRate_RecordRecognition(0);
                Logger_Print(LOG_WARN, "Camera unavailable, skipping inference");
            }

            /* ---- Step 4: 结果回传 H7 ---- */
            Protocol_SendResult(&res);
            g_current_process = PROC_IDLE;
        }
        break;

    /* ====================================================================
     * PROC_AUX_SCAN — 辅助扫描模式（保留，可用作被动扫描时的回退）
     *
     * 当前实现为简化版本，仅在必要时使用。
     * ====================================================================
     */
    case PROC_AUX_SCAN:
        {
            SectorEnergy_t sectors[ROI_SECTOR_COUNT];
            uint8_t cnt = ROI_Optimize_CoarseScan(sectors, ROI_SECTOR_COUNT);

            ResultPacket_t res;
            memset(&res, 0, sizeof(res));
            if (cnt > 0) {
                float max_energy = 0.0f;
                uint8_t best_id = 0;
                for (int i = 0; i < cnt; i++) {
                    if (sectors[i].energy > max_energy) {
                        max_energy = sectors[i].energy;
                        best_id = sectors[i].sector_id;
                    }
                }
                res.confidence = 0.5f;
                res.angle_deg  = -45.0f + (best_id + 0.5f) * (90.0f / ROI_SECTOR_COUNT);
                res.dist_cm    = 0.0f;
            }
            Protocol_SendResult(&res);
            g_current_process = PROC_IDLE;
        }
        break;
    }
}
