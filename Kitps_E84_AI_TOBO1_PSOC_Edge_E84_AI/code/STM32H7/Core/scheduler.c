/**
 * @file    scheduler.c
 * @brief   调度器实现：雷达 -> 84E识别 -> ESP32跟踪 三进程轮转
 */

#include "scheduler.h"
#include "main.h"
#include "radar_process.h"
#include "radar_filter.h"
#include "target_detect.h"
#include "kalman_filter.h"
#include "data_fusion.h"
#include "target_list.h"
#include "target_state.h"
#include "target_switch.h"
#include "protocol.h"
#include "cmd_84e.h"
#include "cmd_esp32.h"
#include "closed_loop.h"
#include "relock_logic.h"
#include "assist_localize.h"
#include "data_exchange.h"
#include "servo.h"
#include "logger.h"
#include "delay_record.h"
#include "error_rate.h"
#include <string.h>
#include <math.h>

static TargetList_t    g_targetList;
static Kalman2D_t      g_kalman[TARGET_MAX_COUNT];
static DataFusion_t    g_fusion;

static RawPoint_t       g_rawPoints[RAW_POINTS_MAX];
static RawPoint_t       g_filteredPoints[RAW_POINTS_MAX];
static TargetCandidate_t g_candidates[TARGET_MAX_PER_FRAME];

static Process_t       g_currentProc = PROC_RADAR_SCAN;
static uint32_t        g_lastScanTick = 0;

/* 连续旋转舵机状态 */
static uint32_t        g_sweepPhaseStart = 0;
static float           g_trackAngle = -1.0f;

/* 辅助跟踪标志（被 assist_localize.c 引用） */
uint8_t g_assist_direct_mode = 0;

static void Process_Radar(void);
static void Process_InvokeESP32(TrackedTarget_t *t);
static void Process_CheckClosedLoop(void);
static void Process_NetworkSwitch(void);

static void On84EResult(uint8_t id, float conf, float x, float y, float dist, float angle);
static void OnESP32Result(uint8_t id, float x, float y, uint8_t lost);

void Scheduler_Init(void)
{
    Logger_Print(LOG_INFO, "Scheduler Init...");

    TargetList_Init(&g_targetList);
    for (int i = 0; i < TARGET_MAX_COUNT; i++) {
        Kalman2D_Init(&g_kalman[i], 0.05f, 0.1f, 0.1f, 5.0f, 2.0f);
    }
    DataFusion_Init(&g_fusion);

    Protocol_Init();
    Protocol_Register84ECallback(On84EResult);
    Protocol_RegisterESP32Callback(OnESP32Result);

    Cmd_84E_Init();
    Cmd_ESP32_Init();

    ClosedLoop_Init();
    RelockLogic_Init();
    AssistLocalize_Init();
    DataExchange_Init();

    /* 启动雷达连续旋转 - 初始左转 */
    Servo_SetDirect(SERVO_FULLLEFT_CCR);
    g_sweepPhaseStart = HAL_GetTick();

    g_currentProc  = PROC_RADAR_SCAN;
    g_lastScanTick = HAL_GetTick();
    Logger_Print(LOG_INFO, "Scheduler Ready.");
}

void Scheduler_Run(void)
{
    Protocol_ProcessIncoming();

    switch (g_currentProc) {
    case PROC_RADAR_SCAN:
        Process_Radar();
        break;
    case PROC_84E_RECOG:
        break;
    case PROC_ESP32_TRACK:
        break;
    case PROC_IDLE:
        if (HAL_GetTick() - g_lastScanTick >= 60) {
            g_currentProc = PROC_RADAR_SCAN;
        }
        break;
    }

    Process_CheckClosedLoop();
    Process_NetworkSwitch();
}

/* 雷达扫描 + 连续旋转切换 */
static void Process_Radar(void)
{
    uint32_t now = HAL_GetTick();

    /* 连续旋转舵机方向切换 */
    if (g_trackAngle < 0.0f) {
        if (g_sweepPhaseStart == 0 || (now - g_sweepPhaseStart) >= SERVO_TIME_20MS) {
            static int phase = 0;
            static const uint32_t ccr_vals[4] = {
                SERVO_FULLLEFT_CCR,   /* 左转 */
                SERVO_FULLRIGHT_CCR,  /* 右转 */
                SERVO_FULLRIGHT_CCR,  /* 再右转 */
                SERVO_FULLLEFT_CCR    /* 左转回 */
            };
            phase = (phase + 1) & 3;
            g_sweepPhaseStart = now;
            Servo_SetDirect(ccr_vals[phase]);
        }
    } else {
        Servo_SetDirect(SERVO_STOP_CCR);
        g_currentProc = PROC_ESP32_TRACK;
        return;
    }

    /* 雷达采集（每200ms） */
    if (now - g_lastScanTick < 200) return;
    g_lastScanTick = now;

    uint8_t cnt = Radar_Process_Scan(g_rawPoints, RAW_POINTS_MAX);
    if (cnt == 0) return;

    cnt = Radar_Filter_Apply(g_rawPoints, cnt);
    cnt = Radar_Filter_GetCleaned(g_filteredPoints, RAW_POINTS_MAX);

    if (cnt > 0) {
        cnt = TargetDetect_Cluster(g_filteredPoints, cnt, g_candidates);
        for (uint8_t i = 0; i < cnt && i < TARGET_MAX_PER_FRAME; i++) {
            if (g_candidates[i].confidence > 0.3f) {
                int8_t idx = TargetList_FindOrAdd(&g_targetList, &g_candidates[i]);
                if (idx >= 0) {
                    TrackedTarget_t *t = &g_targetList.targets[idx];
                    Kalman2D_Predict(&g_kalman[idx], 0.1f, &t->x, &t->y);
                    Cmd_84E_SendScanCmd(t->angle_deg, 20.0f,
                                        t->distance_cm - 50.0f,
                                        t->distance_cm + 50.0f);
                    DelayRecord_Start84E(t->id);
                }
            }
        }
    }

    g_currentProc = PROC_IDLE;
}

static void On84EResult(uint8_t id, float conf, float x, float y, float dist, float angle)
{
    DelayRecord_Stop84E(id);
    DataFusion_UpdateWith84E(&g_fusion, id, conf, x, y, dist, angle);
    TargetState_Update(&g_targetList, id, TARGET_STATE_TRACKED);

    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);
    if (t) {
        t->x = x;
        t->y = y;
        Process_InvokeESP32(t);
        g_currentProc = PROC_ESP32_TRACK;
    } else {
        g_currentProc = PROC_IDLE;
    }
}

static void OnESP32Result(uint8_t id, float x, float y, uint8_t lost)
{
    DataFusion_UpdateWithESP32(&g_fusion, id, x, y, lost);
    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);
    if (!t) return;
    if (lost) {
        TargetState_Update(&g_targetList, id, TARGET_STATE_LOST);
        RelockLogic_Start(t);
    } else {
        t->x = x;
        t->y = y;
    }
}

static void Process_InvokeESP32(TrackedTarget_t *t)
{
    if (!t) return;
    static uint8_t toggle = 0;
    t->esp_assigned = toggle;
    Cmd_ESP32_SendTrackCmd(toggle, t->angle_deg, t->distance_cm, t->id);
    toggle = (toggle + 1) % 2;
}

static void Process_CheckClosedLoop(void)
{
    for (int i = 0; i < g_targetList.count; i++) {
        TrackedTarget_t *t = &g_targetList.targets[i];
        if (t->state == TARGET_STATE_LOST) {
            RelockLogic_Start(t);
        } else if (t->state == TARGET_STATE_TRACKED) {
            ClosedLoop_Check(t);
            if (g_targetList.count == 1) {
                AssistLocalize_Execute(t);
            }
        }
    }
}

static void Process_NetworkSwitch(void)
{
    if (g_targetList.count >= 3) {
        TargetSwitch_Execute(&g_targetList);
        DataExchange_SendTargetList(&g_targetList);
    }
}
