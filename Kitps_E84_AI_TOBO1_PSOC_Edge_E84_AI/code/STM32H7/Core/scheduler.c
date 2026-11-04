/**
 * @file    scheduler.c
 * @brief   调度器实现：雷达 -> 84E识别 -> ESP32跟踪 三进程轮转
 *
 * ESP32 资源以“角色”管理（g_espRole/g_espTarget），它是唯一可信来源：
 *   ESP_ROLE_TRACK    主跟踪板：其回传驱动目标状态机、复锁和边界接力
 *   ESP_ROLE_ASSIST   单目标时的辅助观察板：回传只记录，不改变目标状态
 *   ESP_ROLE_HANDOVER 边界接力的接手板：首次稳定锁定后转为主跟踪板
 *
 * 跟踪命令（CMD_TRACK）只在分配/接力/复锁时下发一次；
 * ESP32 每收到一次 CMD_TRACK 都会复锁并重置跟踪器，严禁周期性重发。
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
static Kalman2D_t      g_kalman[TARGET_MAX_COUNT];   /* 按 target id-1 索引 */
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
static uint32_t        g_lastEspRx[2] = {0, 0};      /* 云台安全看门狗用，超时后清零 */
static uint32_t        g_espLastFrame[2] = {0, 0};   /* 最近一帧回传时刻，不清零 */
static uint32_t        g_espLastResend[2] = {0, 0};
static uint32_t        g_lastRelockAttempt[2] = {0, 0};
static uint32_t        g_lastHistTick[TARGET_MAX_COUNT] = {0};

#define DEFAULT_84E_WIDTH_DEG     20.0f
#define DEFAULT_84E_MARGIN_CM     50.0f
#define RELOCK_INTERVAL_MS        500U
#define HISTORY_INTERVAL_MS       100U
#define HANDOVER_TIMEOUT_MS       3000U
#define RECOG_TIMEOUT_MS          1500U   /* 84E 识别命令发出后无结果的超时 */

typedef struct {
    uint8_t active;
    uint8_t target_id;
    float angle;        /* 目标方位角（相对正前方，°） */
    float distance;     /* cm */
    float width;        /* 仅84E：扫描窗口宽度（°） */
    float margin;       /* 仅84E：距离窗口半宽（cm） */
} PendingAim_t;

static PendingAim_t g_pending84E = {0};
static PendingAim_t g_pendingESP[2] = {{0}, {0}};
static uint8_t g_84EBusy = 0U;
static uint32_t g_84ESentTick = 0U;   /* 0 = 命令尚未发出（云台预转中） */

/* ESP32 角色 */
typedef enum {
    ESP_ROLE_IDLE = 0,
    ESP_ROLE_TRACK,
    ESP_ROLE_ASSIST,
    ESP_ROLE_HANDOVER
} EspRole_t;
static EspRole_t g_espRole[2] = {ESP_ROLE_IDLE, ESP_ROLE_IDLE};
static uint8_t   g_espTarget[2] = {0U, 0U};

/* 辅助板最近一次观测（三角解算尚未实现，先记录供后续使用） */
typedef struct {
    uint8_t target_id;
    float x, y;
    uint8_t lost;
    uint32_t tick;
} AssistObs_t;
static AssistObs_t g_assistObs = {0};

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

static void RecogQueue_Push(const PendingAim_t *aim)
{
    if (aim->target_id == g_active84ETargetId || RecogQueue_Contains(aim->target_id)) return;
    if (g_recogQueue.count >= RECOG_QUEUE_DEPTH) return;  /* 队列满，拒绝新候选 */
    g_recogQueue.items[g_recogQueue.tail] = *aim;
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
    uint32_t start_tick;
} Handover_t;
static Handover_t g_handover = {0};
static uint8_t g_handoverArmed = 1U;
static uint8_t g_shutdownRequested = 0U;
static uint8_t g_shutdownComplete = 0U;
static uint32_t g_radarHomeDeadline = 0U;
static uint8_t g_radarHoming = 0U;

static void Process_Radar(void);
static void RadarSweep_Update(void);
static void GimbalSafety_Update(void);
static void PendingAim_Update(void);
static void Shutdown_Update(void);
static void Process_InvokeESP32(TrackedTarget_t *t);
static void Process_CheckClosedLoop(void);
static void StartHandover(uint8_t from_esp, TrackedTarget_t *t);
static void Handover_Update(void);

static float BearingToPanAngle(float bearing_deg)
{
    float angle = GIMBAL_PAN_INITIAL_ANGLE + bearing_deg;
    if (angle < GIMBAL_PAN_HARD_MIN) angle = GIMBAL_PAN_HARD_MIN;
    if (angle > GIMBAL_PAN_HARD_MAX) angle = GIMBAL_PAN_HARD_MAX;
    return angle;
}

/* 云台当前指向（相对正前方的方位角） */
static float GimbalBearing(GimbalId_t gimbal)
{
    return Gimbal_GetEstimatedPan(gimbal) - GIMBAL_PAN_INITIAL_ANGLE;
}

/* ──── ESP32 角色管理 ──── */
static void EspAssign(uint8_t esp, EspRole_t role, uint8_t target_id)
{
    if (esp > 1U) return;
    g_espRole[esp] = role;
    g_espTarget[esp] = target_id;
}

static void EspRelease(uint8_t esp)
{
    if (esp > 1U) return;
    Cmd_ESP32_SendReleaseCmd(esp);
    Gimbal_ReturnToInitial((GimbalId_t)esp);
    g_espRole[esp] = ESP_ROLE_IDLE;
    g_espTarget[esp] = 0U;
    g_pendingESP[esp].active = 0U;
    g_lastEspRx[esp] = 0U;
}

/* 把 esp 的云台转到 gimbal_bearing，到位后下发一次 CMD_TRACK，
 * 预测位置为目标方位 target_bearing（发送时换算成相对云台指向的偏差）。 */
static void AimESP(uint8_t esp, uint8_t target_id,
                   float target_bearing, float gimbal_bearing, float distance)
{
    if (esp > 1U) return;
    g_pendingESP[esp].active = 1U;
    g_pendingESP[esp].target_id = target_id;
    g_pendingESP[esp].angle = target_bearing;
    g_pendingESP[esp].distance = distance;
    Gimbal_MovePanTo((GimbalId_t)esp, BearingToPanAngle(gimbal_bearing));
}

/* ──── 84E 请求：空闲则预转云台后识别，忙则排队 ──── */
static void Dispatch84E(const PendingAim_t *aim)
{
    g_pending84E = *aim;
    g_pending84E.active = 1U;
    g_84EBusy = 1U;
    g_84ESentTick = 0U;
    g_active84ETargetId = aim->target_id;
    Gimbal_MovePanTo(GIMBAL_CAM2, BearingToPanAngle(aim->angle));
    g_currentProc = PROC_84E_RECOG;
}

static void Request84E(uint8_t target_id, float angle, float distance,
                       float width, float margin)
{
    PendingAim_t aim = {1U, target_id, angle, distance, width, margin};
    if (!g_84EBusy && !g_pending84E.active) {
        Dispatch84E(&aim);
    } else {
        RecogQueue_Push(&aim);
    }
}

static uint8_t Dispatch84EFromQueue(void)
{
    PendingAim_t next;
    if (!RecogQueue_Pop(&next)) return 0U;
    Dispatch84E(&next);
    return 1U;
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
    Handover_Update();

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
    g_handover.active = 0U;
    g_active84ETargetId = 0U;
    memset(&g_recogQueue, 0, sizeof(g_recogQueue));

    EspRelease(0);
    EspRelease(1);
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
                            g_pending84E.angle, g_pending84E.width,
                            g_pending84E.distance - g_pending84E.margin,
                            g_pending84E.distance + g_pending84E.margin);
        DelayRecord_Start84E(g_pending84E.target_id);
        g_84ESentTick = HAL_GetTick();
        g_pending84E.active = 0U;
    }

    /* 84E 无回复：解除忙状态，继续下一候选或回到雷达扫描 */
    if (g_84EBusy && !g_pending84E.active && g_84ESentTick != 0U &&
        (HAL_GetTick() - g_84ESentTick) >= RECOG_TIMEOUT_MS) {
        Logger_Print(LOG_WARN, "84E recog timeout");
        g_84EBusy = 0U;
        g_84ESentTick = 0U;
        g_active84ETargetId = 0U;
        if (!Dispatch84EFromQueue()) g_currentProc = PROC_IDLE;
    }

    for (uint8_t i = 0; i < 2; i++) {
        if (g_pendingESP[i].active &&
            !Gimbal_IsPanMoving((GimbalId_t)i)) {
            /* param1 = 目标相对云台当前指向的偏差角（°）。
             * 云台已转到位，通常接近 0；辅助板为 -偏移量；
             * 目标超出云台硬限位时为剩余偏差。 */
            float rel_angle = g_pendingESP[i].angle - GimbalBearing((GimbalId_t)i);
            Cmd_ESP32_SendTrackCmd(i, rel_angle,
                                   g_pendingESP[i].distance,
                                   g_pendingESP[i].target_id);
            g_espLastResend[i] = HAL_GetTick();
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
                    Kalman2D_Predict(&g_kalman[t->id - 1], 0.1f, &t->x, &t->y);
                    /* 84E空闲则立即预转识别，忙则入队等空闲后自动发送 */
                    Request84E(t->id, t->angle_deg, t->distance_cm,
                               DEFAULT_84E_WIDTH_DEG, DEFAULT_84E_MARGIN_CM);
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
    g_84ESentTick = 0U;
    g_active84ETargetId = 0U;
    DelayRecord_Stop84E(id);
    if (conf <= 0.3f) {
        /* 识别失败：尝试队列中的下一候选 */
        if (!Dispatch84EFromQueue()) {
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
    Dispatch84EFromQueue();
}

/* 释放某目标占用的全部 ESP32 并移出列表 */
static void ReleaseTarget(uint8_t id)
{
    for (uint8_t i = 0; i < 2; i++) {
        if (g_espRole[i] != ESP_ROLE_IDLE && g_espTarget[i] == id) {
            EspRelease(i);
        }
    }
    if (g_handover.active && g_handover.target_id == id) {
        g_handover.active = 0U;
    }
    TargetList_Remove(&g_targetList, id);
    RelockLogic_ResetRetries(id);
    g_currentProc = PROC_IDLE;
}

static void OnPrimaryResult(uint8_t esp_id, TrackedTarget_t *t,
                            float x, float y,
                            float pan_ctrl, float tilt_ctrl, uint8_t lost)
{
    uint32_t now = HAL_GetTick();

    if (lost) {
        Gimbal_SetPanMotion((GimbalId_t)esp_id, GIMBAL_PAN_STOP);
        if (t->state != TARGET_STATE_LOST) {
            TargetState_Update(&g_targetList, t->id, TARGET_STATE_LOST);
            g_lastRelockAttempt[esp_id] = 0U;
        }
        if (g_lastRelockAttempt[esp_id] != 0U &&
            (now - g_lastRelockAttempt[esp_id]) < RELOCK_INTERVAL_MS) {
            return;
        }
        g_lastRelockAttempt[esp_id] = now;

        RelockPlan_t plan;
        if (RelockLogic_Start(t, &plan) == RELOCK_RELEASE) {
            ReleaseTarget(t->id);
            return;
        }
        /* 经调度器请求84E（预转云台、忙则排队），只让本目标的主板转向预测方位复锁 */
        Request84E(t->id, plan.pred_angle, plan.pred_dist,
                   plan.scan_width, plan.dist_margin);
        AimESP(esp_id, t->id, plan.pred_angle, plan.pred_angle, plan.pred_dist);
        return;
    }

    if (t->state != TARGET_STATE_TRACKED) {
        TargetState_Update(&g_targetList, t->id, TARGET_STATE_TRACKED);
    }
    RelockLogic_ResetRetries(t->id);
    g_lastRelockAttempt[esp_id] = 0U;
    t->x = x;
    t->y = y;

    Gimbal_ApplyTrackingControl((GimbalId_t)esp_id, pan_ctrl, tilt_ctrl);
    /* H7 统一控制云台，目标方位以主板云台指向为准 */
    t->angle_deg = GimbalBearing((GimbalId_t)esp_id);
    t->esp_assigned = esp_id;

    uint8_t hi = (uint8_t)(t->id - 1U);
    if (hi < TARGET_MAX_COUNT && (now - g_lastHistTick[hi]) >= HISTORY_INTERVAL_MS) {
        g_lastHistTick[hi] = now;
        RelockLogic_RecordPosition(t->id, t->angle_deg, t->distance_cm);
    }

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

static void OnESP32Result(uint8_t esp_id, uint8_t id,
                          float x, float y,
                          float pan_ctrl, float tilt_ctrl,
                          uint8_t lost)
{
    if (esp_id > 1) return;
    uint32_t now = HAL_GetTick();
    g_lastEspRx[esp_id] = now;
    g_espLastFrame[esp_id] = now;
    DataFusion_UpdateWithESP32(&g_fusion, id, x, y, lost);

    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);
    EspRole_t role = g_espRole[esp_id];

    if (!t || g_espTarget[esp_id] != id) {
        /* 板子没有任务却在回传 → 释放；已改派其他任务时的旧帧 → 忽略 */
        if (role == ESP_ROLE_IDLE && !g_pendingESP[esp_id].active) {
            EspRelease(esp_id);
        }
        return;
    }

    switch (role) {
    case ESP_ROLE_TRACK:
        OnPrimaryResult(esp_id, t, x, y, pan_ctrl, tilt_ctrl, lost);
        break;

    case ESP_ROLE_HANDOVER:
        /* 接手板首次稳定锁定：释放原板，转为主跟踪板 */
        if (!lost && g_handover.active && g_handover.target_id == id &&
            g_handover.to_esp == esp_id) {
            EspRelease(g_handover.from_esp);
            EspAssign(esp_id, ESP_ROLE_TRACK, id);
            g_handover.active = 0U;
            OnPrimaryResult(esp_id, t, x, y, pan_ctrl, tilt_ctrl, lost);
        }
        break;

    case ESP_ROLE_ASSIST:
        /* 辅助板：只记录观测并让自己的云台随动，不改变目标状态 */
        g_assistObs.target_id = id;
        g_assistObs.x = x;
        g_assistObs.y = y;
        g_assistObs.lost = lost;
        g_assistObs.tick = now;
        if (lost) {
            Gimbal_SetPanMotion((GimbalId_t)esp_id, GIMBAL_PAN_STOP);
        } else {
            Gimbal_ApplyTrackingControl((GimbalId_t)esp_id, pan_ctrl, tilt_ctrl);
        }
        break;

    default:
        break;
    }
}

static void StartHandover(uint8_t from_esp, TrackedTarget_t *t)
{
    if (!t || from_esp > 1U || g_handover.active) return;

    uint8_t to_esp = (uint8_t)(1U - from_esp);
    /* 接手板正在跟踪另一个目标时不能抢占 */
    if (g_espRole[to_esp] == ESP_ROLE_TRACK && g_espTarget[to_esp] != t->id) return;

    float bearing = GimbalBearing((GimbalId_t)from_esp);

    g_handover.active = 1U;
    g_handoverArmed = 0U;
    g_handover.target_id = t->id;
    g_handover.from_esp = from_esp;
    g_handover.to_esp = to_esp;
    g_handover.start_tick = HAL_GetTick();

    EspAssign(to_esp, ESP_ROLE_HANDOVER, t->id);
    AimESP(to_esp, t->id, bearing, bearing, t->distance_cm);

    if (!g_84EBusy) {
        Request84E(t->id, bearing, t->distance_cm,
                   DEFAULT_84E_WIDTH_DEG, DEFAULT_84E_MARGIN_CM);
    }
}

/* 接手板迟迟未锁定 → 取消接力，原板继续跟踪 */
static void Handover_Update(void)
{
    if (!g_handover.active) return;
    if ((HAL_GetTick() - g_handover.start_tick) < HANDOVER_TIMEOUT_MS) return;
    uint8_t to_esp = g_handover.to_esp;
    g_handover.active = 0U;
    if (g_espRole[to_esp] == ESP_ROLE_HANDOVER) {
        EspRelease(to_esp);
    }
    Logger_Print(LOG_WARN, "Handover timeout, keep original ESP32");
}

static void Process_InvokeESP32(TrackedTarget_t *t)
{
    if (!t) return;

    /* 已有主跟踪板（如复锁后重新识别）：沿用，只重新瞄准一次 */
    for (uint8_t i = 0; i < 2; i++) {
        if (g_espRole[i] == ESP_ROLE_TRACK && g_espTarget[i] == t->id) {
            t->esp_assigned = i;
            AimESP(i, t->id, t->angle_deg, t->angle_deg, t->distance_cm);
            return;
        }
    }

    /*
     * [IMPROVE 2026-10-06] ESP32分配策略优化
     *
     * 单目标：固定ESP32-A跟踪，ESP32-B辅助定位（避免toggle导致跟踪板漂移）
     * 双目标：按目标在列表中的位置分配
     * 三目标：同双目标，第三目标由H7本地调度
     */
    uint8_t esp_id;
    if (g_targetList.count <= 1) {
        esp_id = 0U;
    } else {
        uint8_t idx = (uint8_t)(t - g_targetList.targets);
        esp_id = (uint8_t)(idx % 2U);
    }

    /* 主跟踪/接手中的板视为占用；辅助板可以被改派。两块都忙时由H7保留目标。 */
    uint8_t busy0 = (g_espRole[esp_id] == ESP_ROLE_TRACK ||
                     g_espRole[esp_id] == ESP_ROLE_HANDOVER);
    if (busy0) esp_id = (uint8_t)(1U - esp_id);
    if (g_espRole[esp_id] == ESP_ROLE_TRACK ||
        g_espRole[esp_id] == ESP_ROLE_HANDOVER) return;

    EspAssign(esp_id, ESP_ROLE_TRACK, t->id);
    t->esp_assigned = esp_id;
    AimESP(esp_id, t->id, t->angle_deg, t->angle_deg, t->distance_cm);
}

static void Process_CheckClosedLoop(void)
{
    static uint32_t last_check_tick = 0;
    uint32_t now = HAL_GetTick();
    if ((now - last_check_tick) < 100U) return;
    last_check_tick = now;

    /* 辅助板的目标已不存在 → 释放 */
    for (uint8_t i = 0; i < 2; i++) {
        if (g_espRole[i] == ESP_ROLE_ASSIST &&
            !TargetList_GetTarget(&g_targetList, g_espTarget[i])) {
            EspRelease(i);
        }
    }

    for (int i = 0; i < g_targetList.count; i++) {
        TrackedTarget_t *t = &g_targetList.targets[i];
        if (t->state != TARGET_STATE_TRACKED || t->esp_assigned > 1U) continue;
        uint8_t p = t->esp_assigned;
        if (g_espRole[p] != ESP_ROLE_TRACK || g_espTarget[p] != t->id) continue;

        /* 链路看门狗：只在主板长时间无回传时限频重发一次 */
        if (!g_pendingESP[p].active &&
            ClosedLoop_NeedsResend(t, now - g_espLastFrame[p],
                                   now - g_espLastResend[p])) {
            AimESP(p, t->id, t->angle_deg, t->angle_deg, t->distance_cm);
        }

        /* 单目标：另一块空闲板启动一次辅助观察（不重复下发） */
        uint8_t other = (uint8_t)(1U - p);
        if (g_targetList.count == 1 && !g_handover.active &&
            g_espRole[other] == ESP_ROLE_IDLE && !g_pendingESP[other].active) {
            EspAssign(other, ESP_ROLE_ASSIST, t->id);
            AimESP(other, t->id, t->angle_deg,
                   AssistLocalize_ComputeBearing(t), t->distance_cm);
        }
    }
}
