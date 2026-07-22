/**
 * @file    relock_logic.c
 * @brief   复锁逻辑实现：运动趋势预测 + 分级复锁 + 资源释放决策
 *
 * 复锁策略：
 *   第1~3次尝试：在预测位置附近用 84E 广角扫描 + 双 ESP32 辅助
 *   第4~5次尝试：扩大搜索范围，若仍失败则标记为"超出范围"释放进程
 *
 * 运动趋势预测：利用保存的最后 N 帧位置(角度,距离)估算速度，
 * 外推当前时刻的目标位置。
 */

#include "relock_logic.h"
#include "cmd_84e.h"
#include "cmd_esp32.h"

#include <math.h>

#ifndef M_PI
#define M_PI  3.14159265358979323846f
#endif

/* ──── 内部状态 ──── */
typedef struct {
    uint8_t retry_count[TARGET_MAX_COUNT]; /* 每个目标的复锁重试计数 */
    float   last_angles[TARGET_MAX_COUNT][3]; /* 最近3帧角度 */
    float   last_dists[TARGET_MAX_COUNT][3];  /* 最近3帧距离 */
    uint8_t hist_idx[TARGET_MAX_COUNT];       /* 环形缓冲索引 */
} RelockState_t;

static RelockState_t s_relock;

void RelockLogic_Init(void)
{
    for (int i = 0; i < TARGET_MAX_COUNT; i++) {
        s_relock.retry_count[i] = 0;
        s_relock.hist_idx[i] = 0;
        for (int j = 0; j < 3; j++) {
            s_relock.last_angles[i][j] = 0.0f;
            s_relock.last_dists[i][j]  = 0.0f;
        }
    }
}

/* ──── 记录一帧历史位置（由 ClosedLoop_Check 或 On84EResult 调用） ──── */
void RelockLogic_RecordPosition(uint8_t target_id, float angle_deg, float dist_cm)
{
    int idx = (int)target_id - 1;
    if (idx < 0 || idx >= TARGET_MAX_COUNT) return;

    uint8_t h = s_relock.hist_idx[idx];
    s_relock.last_angles[idx][h] = angle_deg;
    s_relock.last_dists[idx][h]  = dist_cm;
    s_relock.hist_idx[idx] = (h + 1) % 3;
}

/* ──── 复位重试计数 ──── */
void RelockLogic_ResetRetries(uint8_t target_id)
{
    int idx = (int)target_id - 1;
    if (idx >= 0 && idx < TARGET_MAX_COUNT) {
        s_relock.retry_count[idx] = 0;
    }
}

/* ──── 预测目标当前位置（基于最近3帧线性外推） ──── */
static void PredictPosition(uint8_t target_id,
                            float *pred_angle, float *pred_dist)
{
    int idx = (int)target_id - 1;
    if (idx < 0 || idx >= TARGET_MAX_COUNT) return;

    float a0 = s_relock.last_angles[idx][0];
    float a1 = s_relock.last_angles[idx][1];
    float a2 = s_relock.last_angles[idx][2];
    float d0 = s_relock.last_dists[idx][0];
    float d1 = s_relock.last_dists[idx][1];
    float d2 = s_relock.last_dists[idx][2];

    /* 角度变化率 (简化：取最近两帧的差值) */
    float angle_vel = (a1 - a0) * 0.5f + (a2 - a1) * 0.5f;
    float dist_vel  = (d1 - d0) * 0.5f + (d2 - d1) * 0.5f;

    *pred_angle = a2 + angle_vel;  /* 外推一帧 */
    *pred_dist  = d2 + dist_vel;

    /* 限幅 */
    if (*pred_angle < 0.0f)   *pred_angle += 360.0f;
    if (*pred_angle > 360.0f) *pred_angle -= 360.0f;
    if (*pred_dist < 5.0f)    *pred_dist = 5.0f;
    if (*pred_dist > 400.0f)  *pred_dist = 400.0f;
}

/* ──── 复锁主入口 ──── */
int RelockLogic_Start(TrackedTarget_t *t)
{
    if (!t) return RELOCK_RELEASE;

    int idx = (int)t->id - 1;
    if (idx < 0 || idx >= TARGET_MAX_COUNT) return RELOCK_RELEASE;

    uint8_t n = s_relock.retry_count[idx];
    float pred_angle = t->angle_deg;
    float pred_dist  = t->distance_cm;

    /* 如果有历史数据，用运动趋势预测当前目标位置 */
    if (n > 0) {
        PredictPosition(t->id, &pred_angle, &pred_dist);
    }

    n++;
    s_relock.retry_count[idx] = n;

    /* ──── 分级复锁 ──── */
    if (n >= RELOCK_MAX_RETRIES) {
        /* 超过最大重试次数 → 目标已超出范围，释放 */
        s_relock.retry_count[idx] = 0;
        return RELOCK_RELEASE;
    }

    if (n <= 2) {
        /* 第1~2次：预测位置附近±30°广角扫描 */
        Cmd_84E_SendScanCmd(pred_angle, 60.0f,
                            pred_dist - 100.0f, pred_dist + 100.0f);
        Cmd_ESP32_SendTrackCmd(0, pred_angle, pred_dist, t->id);
        Cmd_ESP32_SendTrackCmd(1, pred_angle, pred_dist, t->id);
    } else {
        /* 第3~4次：扩大范围, ±60°, ±200cm */
        Cmd_84E_SendScanCmd(pred_angle, 120.0f,
                            pred_dist - 150.0f, pred_dist + 150.0f);
        Cmd_ESP32_SendTrackCmd(0, pred_angle, pred_dist, t->id);
        Cmd_ESP32_SendTrackCmd(1, pred_angle, pred_dist, t->id);
    }

    return RELOCK_RETRY;
}
