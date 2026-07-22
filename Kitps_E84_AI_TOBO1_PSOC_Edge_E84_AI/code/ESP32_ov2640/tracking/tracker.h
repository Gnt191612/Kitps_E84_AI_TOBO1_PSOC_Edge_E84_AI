/**
 * @file tracker.h
 * @brief 跟踪控制器：区分跟踪、丢失、复锁状态
 *
 * 接口: Tracker_Init, Tracker_Process, Tracker_GetResult
 */

#ifndef TRACKER_H
#define TRACKER_H

#include <stdint.h>
#include "algorithm/track_algorithm.h"
#include "Vision/roi/roi_select.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ──── 丢失标志 ──── */
#define LOST_NORMAL     0   /* 正常跟踪 */
#define LOST_TARGET     1   /* 目标丢失 */

/* ──── 跟踪状态 ──── */
typedef enum {
    TRACK_STATE_IDLE = 0,        /* 空闲 (未开始跟踪) */
    TRACK_STATE_TRACKING,        /* 正常跟踪 */
    TRACK_STATE_LOST,            /* 目标丢失 */
    TRACK_STATE_RELOCKING,       /* 复锁中 */
} TrackState_t;

/* ──── 跟踪器句柄 ──── */
typedef struct {
    TrackState_t state;
    TrackAlgo_t algorithm;
    uint8_t target_id;
    float result_x_mm;       /* 最终结果 X (mm) */
    float result_y_mm;       /* 最终结果 Y (mm) */
    uint8_t lost_flag;
    int lost_counter;        /* 连续丢失帧计数 */
    int relock_attempts;     /* 复锁尝试次数 */

    /* 帧缓存 (灰度帧) */
    uint8_t *frame_buf;
    int frame_w;
    int frame_h;
} Tracker_t;

/* ──── 公共接口 ──── */

/**
 * @brief 初始化跟踪器
 * @param tracker 跟踪器指针
 * @param target_id 目标编号
 * @param w 图像宽度
 * @param h 图像高度
 */
void Tracker_Init(Tracker_t *tracker, uint8_t target_id, int w, int h);

/**
 * @brief 处理一帧 (由调度器每帧调用)
 * @param tracker 跟踪器指针
 * @param id      目标编号 (用于校验)
 * @param frame   输入灰度帧
 * @return 当前状态
 */
TrackState_t Tracker_Process(Tracker_t *tracker, uint8_t id, const uint8_t *frame);

/**
 * @brief 获取跟踪结果
 * @param tracker 跟踪器指针
 * @param x       输出 X (mm)
 * @param y       输出 Y (mm)
 * @param lost    输出丢失标志
 */
void Tracker_GetResult(Tracker_t *tracker, float *x, float *y, uint8_t *lost);

/**
 * @brief 开始跟踪
 * @param tracker 跟踪器指针
 */
void Tracker_Start(Tracker_t *tracker);

/**
 * @brief 停止跟踪 (释放进程)
 * @param tracker 跟踪器指针
 */
void Tracker_Stop(Tracker_t *tracker);

/**
 * @brief 触发复锁
 * @param tracker 跟踪器指针
 */
void Tracker_Relock(Tracker_t *tracker);

#ifdef __cplusplus
}
#endif

#endif /* TRACKER_H */
