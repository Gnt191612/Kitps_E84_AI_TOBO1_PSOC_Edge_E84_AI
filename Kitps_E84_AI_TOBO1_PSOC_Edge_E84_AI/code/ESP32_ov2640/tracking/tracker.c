/**
 * @file tracker.c
 * @brief 跟踪控制器实现
 */

#include "tracker.h"
#include "tracking/closed_loop.h"
#include "tracking/relock.h"
#include "data_logger/logger.h"
#include <string.h>

#define LOST_THRESHOLD      5   /* 连续 N 帧丢失判定丢失 */
#define RELOCK_MAX_ATTEMPTS 3

/* ──── 初始化 ──── */
void Tracker_Init(Tracker_t *tracker, uint8_t target_id, int w, int h)
{
    if (!tracker) return;
    memset(tracker, 0, sizeof(Tracker_t));

    tracker->state = TRACK_STATE_IDLE;
    tracker->target_id = target_id;
    tracker->lost_flag = LOST_NORMAL;
    tracker->lost_counter = 0;
    tracker->relock_attempts = 0;
    tracker->frame_w = w;
    tracker->frame_h = h;

    /* 分配帧缓存 */
    if (!tracker->frame_buf) {
        tracker->frame_buf = (uint8_t *)malloc(w * h);
    }

    TrackAlgo_Init(&tracker->algorithm);
    ClosedLoop_Init();
    Relock_Init();

    LOG_INFO("Tracker[%d] init OK, %dx%d", target_id, w, h);
}

/* ──── 开始 ──── */
void Tracker_Start(Tracker_t *tracker)
{
    if (!tracker) return;
    tracker->state = TRACK_STATE_TRACKING;
    tracker->lost_counter = 0;
    tracker->lost_flag = LOST_NORMAL;
    TrackAlgo_Reset(&tracker->algorithm);
    LOG_INFO("Tracker[%d] started", tracker->target_id);
}

/* ──── 停止 ──── */
void Tracker_Stop(Tracker_t *tracker)
{
    if (!tracker) return;
    tracker->state = TRACK_STATE_IDLE;
    tracker->lost_flag = LOST_TARGET;
    TrackAlgo_Reset(&tracker->algorithm);
    LOG_INFO("Tracker[%d] stopped", tracker->target_id);
}

/* ──── 复锁 ──── */
void Tracker_Relock(Tracker_t *tracker)
{
    if (!tracker) return;
    tracker->state = TRACK_STATE_RELOCKING;
    tracker->relock_attempts = 0;
    LOG_INFO("Tracker[%d] relock triggered", tracker->target_id);
}

/* ──── 处理一帧 ──── */
TrackState_t Tracker_Process(Tracker_t *tracker, uint8_t id, const uint8_t *frame)
{
    if (!tracker || !frame) return tracker ? tracker->state : TRACK_STATE_IDLE;

    /* 保存帧 */
    if (tracker->frame_buf && frame != tracker->frame_buf) {
        memcpy(tracker->frame_buf, frame, tracker->frame_w * tracker->frame_h);
    }

    switch (tracker->state) {
    case TRACK_STATE_IDLE:
        /* 空闲状态不做处理 */
        tracker->lost_flag = LOST_TARGET;
        break;

    case TRACK_STATE_TRACKING: {
        float x, y;
        int ret = TrackAlgo_Process(&tracker->algorithm, frame,
                                    tracker->frame_w, tracker->frame_h,
                                    &x, &y);

        if (ret == 0) {
            /* 跟踪成功 */
            tracker->lost_counter = 0;
            tracker->lost_flag = LOST_NORMAL;
            tracker->result_x_mm = x;
            tracker->result_y_mm = y;

            /* 闭环控制 */
            ClosedLoop_Update(x, y);
        } else {
            /* 跟踪失败 */
            tracker->lost_counter++;
            tracker->lost_flag = LOST_TARGET;

            if (tracker->lost_counter >= LOST_THRESHOLD) {
                tracker->state = TRACK_STATE_LOST;
                ClosedLoop_Reset();
                LOG_WARN("Tracker[%d] target lost!", tracker->target_id);
            }
        }
        break;
    }

    case TRACK_STATE_LOST: {
        tracker->lost_flag = LOST_TARGET;

        /* 自动尝试复锁 */
        if (tracker->relock_attempts < RELOCK_MAX_ATTEMPTS) {
            int ret = Relock_Execute(frame, tracker->frame_w, tracker->frame_h,
                                     &tracker->result_x_mm, &tracker->result_y_mm);
            if (ret == 0) {
                /* 复锁成功 */
                tracker->state = TRACK_STATE_TRACKING;
                tracker->lost_counter = 0;
                tracker->lost_flag = LOST_NORMAL;
                LOG_INFO("Tracker[%d] relock success!", tracker->target_id);
            } else {
                tracker->relock_attempts++;
            }
        }
        break;
    }

    case TRACK_STATE_RELOCKING: {
        int ret = Relock_Execute(frame, tracker->frame_w, tracker->frame_h,
                                 &tracker->result_x_mm, &tracker->result_y_mm);
        if (ret == 0) {
            tracker->state = TRACK_STATE_TRACKING;
            tracker->lost_counter = 0;
            tracker->lost_flag = LOST_NORMAL;
            LOG_INFO("Tracker[%d] relock success!", tracker->target_id);
        } else {
            tracker->relock_attempts++;
            if (tracker->relock_attempts >= RELOCK_MAX_ATTEMPTS) {
                tracker->state = TRACK_STATE_LOST;
                LOG_WARN("Tracker[%d] relock failed, went to LOST", tracker->target_id);
            }
        }
        break;
    }

    default:
        break;
    }

    return tracker->state;
}

/* ──── 获取结果 ──── */
void Tracker_GetResult(Tracker_t *tracker, float *x, float *y, uint8_t *lost)
{
    if (!tracker) return;
    if (x) *x = tracker->result_x_mm;
    if (y) *y = tracker->result_y_mm;
    if (lost) *lost = tracker->lost_flag;
}
