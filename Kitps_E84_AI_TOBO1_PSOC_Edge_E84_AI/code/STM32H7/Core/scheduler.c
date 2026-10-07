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
#include "gimbal.h"
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

/* 270°位置舵机扫描状态 */
typedef enum {
    SWEEP_LEFT_OUT = 0,
    SWEEP_LEFT_RETURN,
    SWEEP_RIGHT_OUT,
    SWEEP_RIGHT_RETURN
} SweepPhase_t;

static uint32_t        g_sweepPhaseStart = 0;
static SweepPhase_t    g_sweepPhase = SWEEP_LEFT_OUT;
static uint32_t        g_lastEspRx[2] = {0, 0};
static uint32_t        g_lastRelockAttempt[2] = {0, 0};

typedef struct {
    uint8_t active;
    uint8_t target_id;
    float angle;
    float distance;
} PendingAim_t;

static PendingAim_t g_pending84E = {0};
static PendingAim_t g_pendingESP[2] = {{0}, {0}};
static uint8_t g_84EBusy = 0U;

/* 84E候选目标排队（FIFO，深度=TARGET_MAX_COUNT）
 * [IMPROVE 2026-10-06] 原设计多候选时仅发第一个到84E，后续丢弃。
 * 现改为排队，84E空闲后自动发送下一候选。 */
#define RECOG_QUEUE_DEPTH  TARGET_MAX_COUNT
typedef struct {
    PendingAim_t items[RECOG_QUEUE_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} RecogQueue_t;
static RecogQueue_t g_recogQueue = {0};
static uint8_t g_active84ETargetId = 0U;

static uint8_t RecogQueue_Contains(uint8_t target_id)
{
    for (uint8_t i = 0U; i < g_recogQueue.count; i++) {
        uint8_t index = (uint8_t)((g_recogQueue.head + i) % RECOG_QUEUE_DEPTH);
        if (g_recogQueue.items[index].target_id == target_id) return 1U;
    }
    return 0U;
}

static void RecogQueue_Push(uint8_t target_id, float angle, float distance)
{
    if (target_id == g_active84ETargetId || RecogQueue_Contains(target_id)) return;
    if (g_recogQueue.count >= RECOG_QUEUE_DEPTH) return;  /* 队列满，拒绝新候选 */
    g_recogQueue.items[g_recogQueue.tail].target_id = target_id;
    g_recogQueue.items[g_recogQueue.tail].angle = angle;
    g_recogQueue.items[g_recogQueue.tail].distance = distance;
    g_recogQueue.items[g_recogQueue.tail].active = 1U;
    g_recogQueue.tail = (g_recogQueue.tail + 1U) % RECOG_QUEUE_DEPTH;
    g_recogQueue.count++;
}

static uint8_t RecogQueue_Pop(PendingAim_t *out)
{
    if (g_recogQueue.count == 0) return 0U;
    *out = g_recogQueue.items[g_recogQueue.head];
    g_recogQueue.head = (g_recogQueue.head + 1U) % RECOG_QUEUE_DEPTH;
    g_recogQueue.count--;
    return 1U;
}
typedef struct {
    uint8_t active;
    uint8_t target_id;
    uint8_t from_esp;
    uint8_t to_esp;
} Handover_t;
static Handover_t g_handover = {0};
static uint8_t g_handoverArmed = 1U;
static uint8_t g_shutdownRequested = 0U;
static uint8_t g_shutdownComplete = 0U;
static uint32_t g_radarHomeDeadline = 0U;
static uint8_t g_radarHoming = 0U;

/* 辅助跟踪标志（被 assist_localize.c 引用） */
uint8_t g_assist_direct_mode = 0;

static void Process_Radar(void);
static void RadarSweep_Update(void);
static void GimbalSafety_Update(void);
static void PendingAim_Update(void);
static void Shutdown_Update(void);
static void Process_InvokeESP32(TrackedTarget_t *t);
static void Process_CheckClosedLoop(void);
static void StartHandover(uint8_t from_esp, TrackedTarget_t *t);

static float BearingToPanAngle(float bearing_deg)
{
    float angle = GIMBAL_PAN_INITIAL_ANGLE + bearing_deg;
    if (angle < GIMBAL_PAN_HARD_MIN) angle = GIMBAL_PAN_HARD_MIN;
    if (angle > GIMBAL_PAN_HARD_MAX) angle = GIMBAL_PAN_HARD_MAX;
    return angle;
}

static void On84EResult(uint8_t id, float conf, float x, float y, float dist, float angle);
static void OnESP32Result(uint8_t esp_id, uint8_t id,
                          float x, float y,
                          float pan_ctrl, float tilt_ctrl,
                          uint8_t lost);

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

    /* 先保持135°初始朝向，再进入左侧扫描段。 */
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    g_sweepPhaseStart = HAL_GetTick();

    g_currentProc  = PROC_RADAR_SCAN;
    g_lastScanTick = HAL_GetTick();
    Logger_Print(LOG_INFO, "Scheduler Ready.");
}

void Scheduler_Run(void)
{
    Protocol_ProcessIncoming();
    Gimbal_Update();

    if (g_shutdownRequested) {
        Shutdown_Update();
        return;
    }

    RadarSweep_Update();
    GimbalSafety_Update();
    PendingAim_Update();

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
}

void Scheduler_RequestShutdown(void)
{
    if (g_shutdownRequested) return;

    g_shutdownRequested = 1U;
    g_shutdownComplete = 0U;
    g_pending84E.active = 0U;
    g_pendingESP[0].active = 0U;
    g_pendingESP[1].active = 0U;
    g_handover.active = 0U;
    g_active84ETargetId = 0U;
    memset(&g_recogQueue, 0, sizeof(g_recogQueue));

    Cmd_ESP32_SendReleaseCmd(0);
    Cmd_ESP32_SendReleaseCmd(1);
    for (uint8_t i = 0; i < GIMBAL_COUNT; i++) {
        Gimbal_ReturnToInitial((GimbalId_t)i);
    }

    float home_error = Servo_GetAngle() - SERVO_ANGLE_CENTER;
    if (home_error < 0.0f) home_error = -home_error;
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    if (home_error < 1.0f) {
        g_radarHoming = 0U;
    } else {
        g_radarHomeDeadline = HAL_GetTick() + SERVO_HOME_SETTLE_MS;
        g_radarHoming = 1U;
    }
}

uint8_t Scheduler_IsShutdownComplete(void)
{
    return g_shutdownComplete;
}

static void RadarSweep_Update(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - g_sweepPhaseStart;
    if (elapsed > SERVO_SWEEP_LEG_MS) elapsed = SERVO_SWEEP_LEG_MS;
    float progress = (float)elapsed / (float)SERVO_SWEEP_LEG_MS;
    float angle = SERVO_ANGLE_CENTER;

    switch (g_sweepPhase) {
    case SWEEP_LEFT_OUT:     angle = 135.0f * (1.0f - progress); break;
    case SWEEP_LEFT_RETURN:  angle = 135.0f * progress; break;
    case SWEEP_RIGHT_OUT:    angle = 135.0f + 135.0f * progress; break;
    case SWEEP_RIGHT_RETURN: angle = 270.0f - 135.0f * progress; break;
    }
    Servo_SetAngle(angle);

    if (elapsed < SERVO_SWEEP_LEG_MS) return;

    g_sweepPhase = (SweepPhase_t)((g_sweepPhase + 1) & 3);
    g_sweepPhaseStart = now;
}

static void GimbalSafety_Update(void)
{
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < 2; i++) {
        if (g_lastEspRx[i] != 0 && (now - g_lastEspRx[i]) > 250U) {
            Gimbal_SetPanMotion((GimbalId_t)i, GIMBAL_PAN_STOP);
            g_lastEspRx[i] = 0;
        }
    }
}

static void PendingAim_Update(void)
{
    if (g_pending84E.active && !Gimbal_IsPanMoving(GIMBAL_CAM2)) {
        Cmd_84E_SendScanCmd(g_pending84E.target_id,
                            g_pending84E.angle, 20.0f,
                            g_pending84E.distance - 50.0f,
                            g_pending84E.distance + 50.0f);
        DelayRecord_Start84E(g_pending84E.target_id);
        g_pending84E.active = 0U;
    }

    for (uint8_t i = 0; i < 2; i++) {
        if (g_pendingESP[i].active &&
            !Gimbal_IsPanMoving((GimbalId_t)i)) {
            Cmd_ESP32_SendTrackCmd(i, g_pendingESP[i].angle,
                                   g_pendingESP[i].distance,
                                   g_pendingESP[i].target_id);
            g_pendingESP[i].active = 0U;
        }
    }
}

static void Shutdown_Update(void)
{
    if (g_radarHoming &&
        (int32_t)(HAL_GetTick() - g_radarHomeDeadline) >= 0) {
        Servo_SetAngle(SERVO_ANGLE_CENTER);
        g_radarHoming = 0U;
    }

    if (!g_radarHoming &&
        !Gimbal_IsPanMoving(GIMBAL_CAM0) &&
        !Gimbal_IsPanMoving(GIMBAL_CAM1) &&
        !Gimbal_IsPanMoving(GIMBAL_CAM2)) {
        Servo_SetAngle(SERVO_ANGLE_CENTER);
        for (uint8_t i = 0; i < GIMBAL_COUNT; i++) {
            Gimbal_SetPanMotion((GimbalId_t)i, GIMBAL_PAN_STOP);
        }
        g_shutdownComplete = 1U;
    }
}

/* 雷达采集与目标检测 */
static void Process_Radar(void)
{
    uint32_t now = HAL_GetTick();

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
                    if (t->state == TARGET_STATE_TRACKED) continue;
                    Kalman2D_Predict(&g_kalman[idx], 0.1f, &t->x, &t->y);
                    if (!g_84EBusy && !g_pending84E.active) {
                        /* 84E空闲：立即发送 */
                        g_pending84E.active = 1U;
                        g_pending84E.target_id = t->id;
                        g_pending84E.angle = t->angle_deg;
                        g_pending84E.distance = t->distance_cm;
                        g_84EBusy = 1U;
                        g_active84ETargetId = t->id;
                        Gimbal_MovePanTo(GIMBAL_CAM2,
                                         BearingToPanAngle(t->angle_deg));
                        g_currentProc = PROC_84E_RECOG;
                    } else {
                        /* 84E忙：候选入队，等空闲后自动发送 */
                        RecogQueue_Push(t->id, t->angle_deg, t->distance_cm);
                    }
                }
            }
        }
    }

    if (!g_pending84E.active) {
        g_currentProc = PROC_IDLE;
    }
}

static void On84EResult(uint8_t id, float conf, float x, float y, float dist, float angle)
{
    g_84EBusy = 0U;
    g_active84ETargetId = 0U;
    DelayRecord_Stop84E(id);
    if (conf <= 0.3f) {
        /* 识别失败：尝试队列中的下一候选 */
        PendingAim_t next;
        if (RecogQueue_Pop(&next)) {
            g_pending84E.active = 1U;
            g_pending84E.target_id = next.target_id;
            g_pending84E.angle = next.angle;
            g_pending84E.distance = next.distance;
            g_84EBusy = 1U;
            g_active84ETargetId = next.target_id;
            Gimbal_MovePanTo(GIMBAL_CAM2, BearingToPanAngle(next.angle));
            g_currentProc = PROC_84E_RECOG;
        } else {
            g_currentProc = PROC_IDLE;
        }
        return;
    }
    DataFusion_UpdateWith84E(&g_fusion, id, conf, x, y, dist, angle);
    TargetState_Update(&g_targetList, id, TARGET_STATE_TRACKED);

    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);
    if (t) {
        t->x = x;
        t->y = y;
        if (!(g_handover.active && g_handover.target_id == id)) {
            Process_InvokeESP32(t);
        }
        g_currentProc = PROC_ESP32_TRACK;
    } else {
        g_currentProc = PROC_IDLE;
    }

    /* 识别成功后：若队列中还有候选，安排84E继续识别 */
    if (g_recogQueue.count > 0) {
        PendingAim_t next;
        if (RecogQueue_Pop(&next)) {
            g_pending84E.active = 1U;
            g_pending84E.target_id = next.target_id;
            g_pending84E.angle = next.angle;
            g_pending84E.distance = next.distance;
            g_84EBusy = 1U;
            g_active84ETargetId = next.target_id;
            Gimbal_MovePanTo(GIMBAL_CAM2, BearingToPanAngle(next.angle));
            g_currentProc = PROC_84E_RECOG;
        }
    }
}

static void OnESP32Result(uint8_t esp_id, uint8_t id,
                          float x, float y,
                          float pan_ctrl, float tilt_ctrl,
                          uint8_t lost)
{
    if (esp_id > 1) return;
    g_lastEspRx[esp_id] = HAL_GetTick();
    DataFusion_UpdateWithESP32(&g_fusion, id, x, y, lost);
    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);
    if (!t) {
        Gimbal_ReturnToInitial((GimbalId_t)esp_id);
        return;
    }
    if (lost) {
        Gimbal_SetPanMotion((GimbalId_t)esp_id, GIMBAL_PAN_STOP);
        if (t->state != TARGET_STATE_LOST) {
            TargetState_Update(&g_targetList, id, TARGET_STATE_LOST);
            g_lastRelockAttempt[esp_id] = 0U;
        }
        uint32_t now = HAL_GetTick();
        if (g_lastRelockAttempt[esp_id] == 0U ||
            (now - g_lastRelockAttempt[esp_id]) >= 500U) {
            g_lastRelockAttempt[esp_id] = now;
            if (RelockLogic_Start(t) == RELOCK_RELEASE) {
                Cmd_ESP32_SendReleaseCmd(esp_id);
                Gimbal_ReturnToInitial((GimbalId_t)esp_id);
                TargetList_Remove(&g_targetList, id);
                g_currentProc = PROC_IDLE;
            }
        }
    } else {
        RelockLogic_ResetRetries(id);
        g_lastRelockAttempt[esp_id] = 0U;
        t->x = x;
        t->y = y;
        RelockLogic_RecordPosition(id, t->angle_deg, t->distance_cm);

        if (g_handover.active && g_handover.target_id == id &&
            g_handover.to_esp == esp_id) {
            Cmd_ESP32_SendReleaseCmd(g_handover.from_esp);
            Gimbal_ReturnToInitial((GimbalId_t)g_handover.from_esp);
            g_lastEspRx[g_handover.from_esp] = 0U;
            t->esp_assigned = esp_id;
            g_handover.active = 0U;
        }

        Gimbal_ApplyTrackingControl((GimbalId_t)esp_id,
                                    pan_ctrl, tilt_ctrl);
        float pan_angle = Gimbal_GetEstimatedPan((GimbalId_t)esp_id);
        if (pan_angle > (GIMBAL_PAN_HANDOVER_LOW + 15.0f) &&
            pan_angle < (GIMBAL_PAN_HANDOVER_HIGH - 15.0f)) {
            g_handoverArmed = 1U;
        }
        if (g_handoverArmed && !g_handover.active &&
            (pan_angle <= GIMBAL_PAN_HANDOVER_LOW ||
             pan_angle >= GIMBAL_PAN_HANDOVER_HIGH)) {
            StartHandover(esp_id, t);
        }
    }
}

static void StartHandover(uint8_t from_esp, TrackedTarget_t *t)
{
    if (!t || from_esp > 1U || g_handover.active) return;

    uint8_t to_esp = (uint8_t)(1U - from_esp);
    float bearing = Gimbal_GetEstimatedPan((GimbalId_t)from_esp) -
                    GIMBAL_PAN_INITIAL_ANGLE;

    g_handover.active = 1U;
    g_handoverArmed = 0U;
    g_handover.target_id = t->id;
    g_handover.from_esp = from_esp;
    g_handover.to_esp = to_esp;

    g_pendingESP[to_esp].active = 1U;
    g_pendingESP[to_esp].target_id = t->id;
    g_pendingESP[to_esp].angle = bearing;
    g_pendingESP[to_esp].distance = t->distance_cm;
    Gimbal_MovePanTo((GimbalId_t)to_esp, BearingToPanAngle(bearing));

    if (!g_84EBusy) {
        g_84EBusy = 1U;
        g_pending84E.active = 1U;
        g_pending84E.target_id = t->id;
        g_pending84E.angle = bearing;
        g_pending84E.distance = t->distance_cm;
        g_active84ETargetId = t->id;
        Gimbal_MovePanTo(GIMBAL_CAM2, BearingToPanAngle(bearing));
    }
}

static void Process_InvokeESP32(TrackedTarget_t *t)
{
    if (!t) return;

    /*
     * [IMPROVE 2026-10-06] ESP32分配策略优化
     *
     * 单目标：固定ESP32-A跟踪，ESP32-B辅助定位（避免toggle导致跟踪板漂移）
     * 双目标：ESP32-A跟踪目标0，ESP32-B跟踪目标1
     * 三目标：同双目标，第三目标由H7本地调度
     */
    uint8_t esp_id;
    if (g_targetList.count <= 1) {
        /* 单目标：始终使用ESP32-A */
        esp_id = 0U;
    } else {
        /* 多目标：按目标在列表中的索引分配 */
        uint8_t idx = (uint8_t)(t - g_targetList.targets);
        esp_id = (uint8_t)(idx % 2U);
    }

    /* 若首选ESP32已被占用则使用另一块；两块都忙时由H7保留目标。 */
    uint8_t occupied[2] = {0U, 0U};
    for (int i = 0; i < g_targetList.count; i++) {
        TrackedTarget_t *other = &g_targetList.targets[i];
        if (other->id != t->id && other->state == TARGET_STATE_TRACKED &&
            other->esp_assigned <= 1U) {
            occupied[other->esp_assigned] = 1U;
        }
    }
    if (occupied[esp_id]) esp_id = (uint8_t)(1U - esp_id);
    if (occupied[esp_id]) return;

    t->esp_assigned = esp_id;
    g_pendingESP[esp_id].active = 1U;
    g_pendingESP[esp_id].target_id = t->id;
    g_pendingESP[esp_id].angle = t->angle_deg;
    g_pendingESP[esp_id].distance = t->distance_cm;
    Gimbal_MovePanTo((GimbalId_t)esp_id,
                     BearingToPanAngle(t->angle_deg));
}

static void Process_CheckClosedLoop(void)
{
    static uint32_t last_cmd_tick = 0;
    uint32_t now = HAL_GetTick();
    if ((now - last_cmd_tick) < 100U) return;
    last_cmd_tick = now;

    for (int i = 0; i < g_targetList.count; i++) {
        TrackedTarget_t *t = &g_targetList.targets[i];
        if (t->state == TARGET_STATE_TRACKED) {
            ClosedLoop_Check(t);
            if (g_targetList.count == 1) {
                AssistLocalize_Execute(t);
            }
        }
    }
}
