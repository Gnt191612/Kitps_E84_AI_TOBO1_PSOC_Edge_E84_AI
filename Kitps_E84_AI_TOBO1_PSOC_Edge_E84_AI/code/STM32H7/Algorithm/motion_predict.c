/**
 * @file    motion_predict.c
 * @brief   目标方位预判实现
 */

#include "motion_predict.h"
#include "kalman_filter.h"
#include "target_list.h"
#include <string.h>
#include <math.h>

#define HIST_LEN   8U

typedef struct {
    Kalman2D_t kf;
    float      bearing_hist[HIST_LEN];
    uint8_t    hist_idx;
    uint8_t    samples;
} MotionState_t;

static MotionState_t s_motion[TARGET_MAX_COUNT];   /* 按 target id-1 索引 */

static MotionState_t *Get(uint8_t target_id)
{
    if (target_id == 0U || target_id > TARGET_MAX_COUNT) return NULL;
    return &s_motion[target_id - 1U];
}

void MotionPredict_Reset(uint8_t target_id)
{
    MotionState_t *m = Get(target_id);
    if (!m) return;
    memset(m, 0, sizeof(*m));
    Kalman2D_Init(&m->kf, MOTION_PREDICT_DT_S, 0.1f, 0.1f, 5.0f, 2.0f);
}

void MotionPredict_Init(void)
{
    for (uint8_t id = 1U; id <= TARGET_MAX_COUNT; id++) {
        MotionPredict_Reset(id);
    }
}

void MotionPredict_Update(uint8_t target_id, float bearing_deg, float dist_cm)
{
    MotionState_t *m = Get(target_id);
    if (!m) return;

    float d, a, vd, va;
    Kalman2D_Update(&m->kf, dist_cm, bearing_deg, &d, &a, &vd, &va);
    if (m->samples == 0U) {
        /* 一维滤波器初值为 0，首帧直接对齐到观测，避免从 0 慢慢收敛 */
        m->kf.kf_dist.x = dist_cm;
        m->kf.kf_angle.x = bearing_deg;
        m->kf.x[0] = dist_cm;
        m->kf.x[1] = bearing_deg;
    }

    m->bearing_hist[m->hist_idx] = bearing_deg;
    m->hist_idx = (uint8_t)((m->hist_idx + 1U) % HIST_LEN);
    if (m->samples < 255U) m->samples++;
}

uint8_t MotionPredict_Get(uint8_t target_id, float horizon_s,
                          float *bearing_deg, float *dist_cm, float *spread_deg)
{
    MotionState_t *m = Get(target_id);
    if (!m || m->samples < MOTION_PREDICT_MIN_SAMPLES) return 0U;

    Kalman2D_Predict(&m->kf, horizon_s, dist_cm, bearing_deg);

    uint8_t n = (m->samples < HIST_LEN) ? m->samples : (uint8_t)HIST_LEN;
    float mean = 0.0f;
    for (uint8_t i = 0; i < n; i++) mean += m->bearing_hist[i];
    mean /= (float)n;
    float var = 0.0f;
    for (uint8_t i = 0; i < n; i++) {
        float e = m->bearing_hist[i] - mean;
        var += e * e;
    }
    *spread_deg = sqrtf(var / (float)n);
    return 1U;
}
