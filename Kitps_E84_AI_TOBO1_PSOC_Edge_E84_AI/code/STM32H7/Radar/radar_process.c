/**
 * @file    radar_process.c
 * @brief   雷达测量调度与候选目标输出实现（非阻塞）
 */

#include "radar_process.h"
#include "radar_map.h"
#include "target_detect.h"
#include "servo.h"
#include "logger.h"
#include <string.h>

static uint8_t  s_initialized = 0;
static uint8_t  s_in_flight = 0;          /* 已触发、等待结果 */
static float    s_trigger_servo_deg = 0;  /* 触发时的舵机角 */
static uint32_t s_last_ping = 0;
static uint8_t  s_bg_reported = 0;

void Radar_Process_Init(void)
{
    Radar_Driver_Init();
    RadarMap_Init();
    s_in_flight = 0;
    s_last_ping = 0;
    s_bg_reported = 0;
    s_initialized = 1;
}

uint8_t Radar_Process_BackgroundReady(void)
{
    return RadarMap_BackgroundReady();
}

void Radar_Process_Absorb(float bearing_deg)
{
    /* 人体在 2 m 处约占 3~4 格，±10° 足以覆盖一个误报物体 */
    RadarMap_Absorb(bearing_deg, 10.0f);
}

uint8_t Radar_Process_Update(struct TargetCandidate_s *out)
{
    if (!s_initialized) return 0;
    uint32_t now = HAL_GetTick();

    if (!s_in_flight) {
        if ((now - s_last_ping) >= RADAR_PING_INTERVAL_MS && Radar_Trigger() == RADAR_OK) {
            s_trigger_servo_deg = Servo_GetAngle();
            s_last_ping = now;
            s_in_flight = 1;
        }
        return 0;
    }

    float dist = 0.0f;
    RadarResult_t r = Radar_Poll(&dist);
    if (r == RADAR_RESULT_NONE) return 0;
    s_in_flight = 0;

    /* 测量期间舵机仍在转动，取触发与回波时刻舵机角的平均 */
    float servo_deg = 0.5f * (s_trigger_servo_deg + Servo_GetAngle());
    int bin = RadarMap_AddMeasurement(servo_deg,
                                      (r == RADAR_RESULT_OK) ? dist : -1.0f, now);

    if (!s_bg_reported && RadarMap_BackgroundReady()) {
        s_bg_reported = 1;
        Logger_Print(LOG_INFO, "Radar background learned");
    }
    if (bin < 0 || r != RADAR_RESULT_OK || !RadarMap_BackgroundReady()) return 0;

    return RadarMap_CandidateAt(bin, now, (TargetCandidate_t *)out);
}
