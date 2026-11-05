/**
 * @file track_algorithm.h
 * @brief 跟踪算法封装：三高特征区域模板匹配 + 预测 + 滤波
 *
 * 首帧在种子位置（H7 下发的预测像素）附近按 3×2 网格选 3 个高梯度区域，
 * 保存 16×16 灰度模板；后续帧在每个区域上一位置 ±TRACK_SEARCH_PX 内做
 * SAD 模板匹配，剔除位移离群的区域，用剩余区域中心的均值作为目标位置。
 * 丢失后用保存的模板在扩大窗口内重新搜索（TrackAlgo_Reacquire）。
 *
 * 对比实验①：EXP_TRACK_FULL_FRAME=1 时改为单个 32×32 模板在全图搜索。
 *
 * 接口: TrackAlgo_Init, TrackAlgo_SetSeed, TrackAlgo_Process, TrackAlgo_Reacquire
 */

#ifndef TRACK_ALGORITHM_H
#define TRACK_ALGORITHM_H

#include <stdint.h>
#include "algorithm/filter.h"
#include "algorithm/predict.h"
#include "Vision/roi/roi_select.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对比实验①开关：0 = 三高特征区域（本项目方法），1 = 全图单模板匹配（对照组） */
#ifndef EXP_TRACK_FULL_FRAME
#define EXP_TRACK_FULL_FRAME   0
#endif

#define TRACK_TPL_SIZE        16    /* 特征区域模板边长（= ROI_DEFAULT_W） */
#define TRACK_FULL_TPL_SIZE   32    /* 全图匹配模板边长 */
#define TRACK_SEARCH_PX        8    /* 逐帧搜索半径 */
#define TRACK_SEED_WIN_W      64    /* 种子附近选特征区域的窗口 */
#define TRACK_SEED_WIN_H      48
#define TRACK_MAD_MAX       22.0f   /* 平均绝对差上限，超过视为匹配失败 */
#define TRACK_OUTLIER_PX       4    /* 与中位位移相差超过该值的区域视为离群 */

/* ──── 跟踪算法句柄 ──── */
typedef struct {
    MeanFilter_t mean_filter;
    KalmanFilter_t kalman_filter;
    Predictor_t predictor;
    ROI_t rois[ROI_MAX_COUNT];
    uint8_t templates[ROI_MAX_COUNT][TRACK_TPL_SIZE * TRACK_TPL_SIZE];
    uint8_t full_tpl[TRACK_FULL_TPL_SIZE * TRACK_FULL_TPL_SIZE];
    int full_x, full_y;       /* 全图模式模板左上角 */
    int roi_count;
    int frame_count;
    int use_kalman;     /* 0=均值滤波, 1=卡尔曼滤波 */
    int seed_x, seed_y; /* 首帧种子像素，-1 = 无（全图选区） */
    float cx, cy;       /* 最近一次目标中心（像素） */
    int has_model;      /* 已建立模板，可用于复锁 */
    uint32_t prev_time_ms; /* 上一帧时刻（卡尔曼 dt） */
} TrackAlgo_t;

/* ──── 公共接口 ──── */

void TrackAlgo_Init(TrackAlgo_t *algo);

/**
 * @brief 设置下一次建模的种子位置（像素）；px/py < 0 表示无预测，全图选区
 */
void TrackAlgo_SetSeed(TrackAlgo_t *algo, int px, int py);

/**
 * @brief 处理一帧图像
 * @return 0=成功, -1=跟踪失败 (目标丢失)
 * 输出坐标为像素 × 1.875 的“伪 mm”，与 H7 config_mount.h 中的换算一致
 */
int TrackAlgo_Process(TrackAlgo_t *algo, const uint8_t *frame, int w, int h,
                      float *result_x, float *result_y);

/**
 * @brief 用已保存的模板在 (cx,cy) 附近 ±radius 像素内重新搜索目标
 * @param cx,cy  搜索中心（像素），<0 时用最近一次目标中心
 * @return 0=找回, -1=未找回或尚无模板
 */
int TrackAlgo_Reacquire(TrackAlgo_t *algo, const uint8_t *frame, int w, int h,
                        int cx, int cy, int radius,
                        float *result_x, float *result_y);

/**
 * @brief 重置跟踪算法 (释放进程时调用)
 */
void TrackAlgo_Reset(TrackAlgo_t *algo);

#ifdef __cplusplus
}
#endif

#endif /* TRACK_ALGORITHM_H */
