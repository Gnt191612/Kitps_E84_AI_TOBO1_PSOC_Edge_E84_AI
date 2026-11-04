/**
 * @file    radar_map.c
 * @brief   按角度分箱的距离图 + 背景建模 + 变化检测实现
 */

#include "radar_map.h"
#include "servo.h"
#include <string.h>
#include <math.h>

static RadarBin_t s_bins[RADAR_BIN_COUNT];

static float BinCenterServoDeg(int bin)
{
    return ((float)bin + 0.5f) * RADAR_BIN_DEG;
}

static float Median3(const float *v, uint8_t n)
{
    float a[RADAR_BG_LEARN_SAMPLES];
    memcpy(a, v, n * sizeof(float));
    /* 插入排序，n 很小 */
    for (uint8_t i = 1; i < n; i++) {
        float x = a[i];
        int j = (int)i - 1;
        while (j >= 0 && a[j] > x) { a[j + 1] = a[j]; j--; }
        a[j + 1] = x;
    }
    return a[n / 2];
}

void RadarMap_Init(void)
{
    memset(s_bins, 0, sizeof(s_bins));
}

uint8_t RadarMap_BackgroundReady(void)
{
    for (int i = 0; i < RADAR_BIN_COUNT - 1; i++) {   /* 最后一格只含 270° 端点，可能扫不到 */
        if (s_bins[i].background_cm <= 0.0f) return 0U;
    }
    return 1U;
}

int RadarMap_AddMeasurement(float servo_deg, float dist_cm, uint32_t now_ms)
{
    if (servo_deg < 0.0f || servo_deg > 270.0f) return -1;
    int bin = (int)(servo_deg / RADAR_BIN_DEG);
    if (bin >= RADAR_BIN_COUNT) bin = RADAR_BIN_COUNT - 1;

    RadarBin_t *b = &s_bins[bin];
    float d = (dist_cm < 0.0f) ? RADAR_FAR_CM : dist_cm;
    b->last_cm = d;

    /* 1. 学习背景 */
    if (b->background_cm <= 0.0f) {
        b->learn_buf[b->learn_cnt++] = d;
        if (b->learn_cnt >= RADAR_BG_LEARN_SAMPLES) {
            b->background_cm = Median3(b->learn_buf, b->learn_cnt);
        }
        return bin;
    }

    /* 2. 变化检测：比背景近一定距离 */
    if (d < b->background_cm - RADAR_CHANGE_CM && d >= RADAR_NEAR_LIMIT_CM) {
        if (!b->changed || (now_ms - b->change_tick) > RADAR_CHANGE_TTL_MS) {
            b->change_hits = 0U;
        }
        b->changed = 1U;
        if (b->change_hits < 255U) b->change_hits++;
        b->change_tick = now_ms;
    } else {
        b->changed = 0U;
        b->change_hits = 0U;
        /* 3. 未变化：缓慢跟随环境（只在不比背景近时更新，避免把人学进背景） */
        b->background_cm += RADAR_BG_ALPHA * (d - b->background_cm);
    }
    return bin;
}

static uint8_t BinActive(int i, uint32_t now_ms)
{
    if (i < 0 || i >= RADAR_BIN_COUNT) return 0U;
    const RadarBin_t *b = &s_bins[i];
    return (b->changed && (now_ms - b->change_tick) <= RADAR_CHANGE_TTL_MS) ? 1U : 0U;
}

uint8_t RadarMap_CandidateAt(int bin, uint32_t now_ms, TargetCandidate_t *out)
{
    if (!out || !BinActive(bin, now_ms)) return 0U;

    /* 向两侧扩展：相邻、仍有效、距离相近的变化格属于同一物体 */
    int lo = bin, hi = bin;
    while (BinActive(lo - 1, now_ms) &&
           fabsf(s_bins[lo - 1].last_cm - s_bins[lo].last_cm) < RADAR_CLUSTER_DIST_CM) lo--;
    while (BinActive(hi + 1, now_ms) &&
           fabsf(s_bins[hi + 1].last_cm - s_bins[hi].last_cm) < RADAR_CLUSTER_DIST_CM) hi++;

    int n = hi - lo + 1;
    uint8_t max_hits = 0;
    float dists[RADAR_BIN_COUNT];
    for (int i = lo; i <= hi; i++) {
        dists[i - lo] = s_bins[i].last_cm;
        if (s_bins[i].change_hits > max_hits) max_hits = s_bins[i].change_hits;
    }
    /* 单格且只出现过一次：可能是噪声，等下一次扫过确认 */
    if (n < 2 && max_hits < 2) return 0U;

    /* 距离取中值 */
    for (int i = 1; i < n; i++) {
        float x = dists[i];
        int j = i - 1;
        while (j >= 0 && dists[j] > x) { dists[j + 1] = dists[j]; j--; }
        dists[j + 1] = x;
    }

    float servo_center = (BinCenterServoDeg(lo) + BinCenterServoDeg(hi)) / 2.0f;
    memset(out, 0, sizeof(*out));
    out->id = 0;
    out->angle_deg = servo_center - SERVO_ANGLE_CENTER;   /* 相对正前方的方位 */
    out->distance_cm = dists[n / 2];
    out->confidence = 0.4f + 0.15f * (float)n + 0.15f * (float)max_hits;
    if (out->confidence > 1.0f) out->confidence = 1.0f;
    out->valid = 1U;
    return 1U;
}

void RadarMap_Absorb(float bearing_deg, float half_width_deg)
{
    float lo_deg = bearing_deg + SERVO_ANGLE_CENTER - half_width_deg;
    float hi_deg = bearing_deg + SERVO_ANGLE_CENTER + half_width_deg;
    for (int i = 0; i < RADAR_BIN_COUNT; i++) {
        float c = BinCenterServoDeg(i);
        if (c < lo_deg || c > hi_deg) continue;
        RadarBin_t *b = &s_bins[i];
        if (b->changed && b->last_cm > 0.0f) {
            b->background_cm = b->last_cm;
        }
        b->changed = 0U;
        b->change_hits = 0U;
    }
}
