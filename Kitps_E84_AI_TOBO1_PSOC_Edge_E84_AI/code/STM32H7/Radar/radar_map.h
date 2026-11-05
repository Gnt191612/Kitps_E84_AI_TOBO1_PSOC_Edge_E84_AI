/**
 * @file    radar_map.h
 * @brief   按角度分箱的距离图 + 背景建模 + 变化检测
 *
 * 思路：雷达本身分不清“墙”和“人”，但人是后来出现的。
 *   1. 把舵机 0°~270° 按 RADAR_BIN_DEG 分格，每格记录背景距离；
 *   2. 启动后先学习背景（每格取前 RADAR_BG_LEARN_SAMPLES 次测量的中值，
 *      无回波按“很远”记）；
 *   3. 之后某格测得的距离比背景近 RADAR_CHANGE_CM 以上，记为“变化格”；
 *   4. 相邻且距离相近的变化格聚成一个候选；单格候选需该格连续两次测量都成立；
 *   5. 未变化的格缓慢更新背景，适应缓慢的环境变化；84E 判为非人体时，
 *      调用 RadarMap_Absorb 把该处当前距离并入背景，以后不再报。
 */
#ifndef __RADAR_MAP_H
#define __RADAR_MAP_H

#include <stdint.h>
#include "target_candidate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 需上板整定的参数 ---- */
#define RADAR_BIN_DEG            5.0f    /* 每格角度宽度（舵机角） */
#define RADAR_BIN_COUNT          55      /* 0~270° 共 54 格，+1 容纳 270° 端点 */
#define RADAR_FAR_CM             450.0f  /* 无回波按此距离记入背景 */
#define RADAR_BG_LEARN_SAMPLES   3       /* 每格学习背景所需样本数（取中值） */
#define RADAR_CHANGE_CM          30.0f   /* 比背景近多少算变化 */
#define RADAR_NEAR_LIMIT_CM      20.0f   /* 小于该距离不报候选（开题“>0.2 m”） */
#define RADAR_BG_ALPHA           0.05f   /* 未变化格的背景更新系数 */
#define RADAR_CHANGE_TTL_MS      8000U   /* 变化格超过该时间未再确认则清除（约一个往返周期） */
#define RADAR_CLUSTER_DIST_CM    50.0f   /* 相邻格距离差小于该值才算同一物体 */

typedef struct {
    float    background_cm;     /* 背景距离，0 = 尚未学好 */
    float    learn_buf[RADAR_BG_LEARN_SAMPLES];
    uint8_t  learn_cnt;
    float    last_cm;           /* 最近一次测量（无回波记 RADAR_FAR_CM） */
    uint8_t  changed;           /* 当前是否为变化格 */
    uint8_t  change_hits;       /* 连续几次扫过都判为变化 */
    uint32_t change_tick;       /* 最近一次判为变化的时刻 */
} RadarBin_t;

void    RadarMap_Init(void);
/* 记入一次测量；servo_deg 为测量时的雷达舵机角，dist_cm<0 表示无回波。
 * 返回被更新的格号（-1 表示角度越界） */
int     RadarMap_AddMeasurement(float servo_deg, float dist_cm, uint32_t now_ms);
/* 以 bin 所在的变化格簇生成候选；返回 1 = 有候选 */
uint8_t RadarMap_CandidateAt(int bin, uint32_t now_ms, TargetCandidate_t *out);
/* 背景是否已全部学好（每格样本数达标） */
uint8_t RadarMap_BackgroundReady(void);
/* 把方位 bearing_deg 附近 ±half_width_deg 的格当前距离并入背景 */
void    RadarMap_Absorb(float bearing_deg, float half_width_deg);

#ifdef __cplusplus
}
#endif

#endif /* __RADAR_MAP_H */
