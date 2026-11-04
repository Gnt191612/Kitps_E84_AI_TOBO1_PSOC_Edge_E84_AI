/**
 * @file    scheduler.c
 * @brief   调度器实现：雷达 -> 84E识别 -> ESP32跟踪 三进程轮转
 *
 * ESP32 资源以“角色”管理（g_espRole/g_espTarget），它是唯一可信来源：
 *   ESP_ROLE_TRACK    主跟踪板：其回传驱动目标状态机、复锁和边界接力
 *   ESP_ROLE_ASSIST   单目标时的辅助观察板：回传用于三角定位，不改变目标状态
 *   ESP_ROLE_HANDOVER 边界接力的接手板：首次稳定锁定后转为主跟踪板
 *
 * 跟踪命令（CMD_TRACK）只在分配/接力/复锁时下发一次；
 * ESP32 每收到一次 CMD_TRACK 都会复锁并重置跟踪器，严禁周期性重发。
 *
 * 角度约定见 geometry.h：方位角 0 = 正前方，顺时针为正；
 * 各云台 Pan 与方位角的换算考虑 config_mount.h 中的安装偏置。
 * 对比实验开关见 exp_config.h，实验事件格式见 exp_log.h。
 */

#include "scheduler.h"
#include "main.h"
#include "radar_process.h"
#include "radar_filter.h"
#include "target_detect.h"
#include "data_fusion.h"
#include "geometry.h"
#include "motion_predict.h"
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
#include "config_mount.h"
#include "exp_config.h"
#include "exp_log.h"
#include "logger.h"
#include "delay_record.h"
#include "error_rate.h"
#include <string.h>
#include <math.h>

static TargetList_t    g_targetList;
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
static uint16_t        g_espFrames[2] = {0, 0};      /* 本秒回传帧数（实验统计） */
static uint16_t        g_espLostFrames[2] = {0, 0};

#define DEFAULT_84E_WIDTH_DEG     20.0f
#define DEFAULT_84E_MARGIN_CM     50.0f
#define MAX_84E_WIDTH_DEG         90.0f
#define RELOCK_INTERVAL_MS        500U
#define HISTORY_INTERVAL_MS       100U
#define HANDOVER_TIMEOUT_MS       3000U
#define RECOG_TIMEOUT_MS          1500U   /* 84E 识别命令发出后无结果的超时 */
#define RECOG_LATENCY_S           0.2f    /* 预判外推时计入的 84E 推理与通信时间 */
#define COVER_MARGIN_DEG          10.0f   /* 判断云台能否覆盖某方位时留的余量 */
#define TRIANGULATE_MAX_SKEW_MS   150U    /* 两路观测时间差上限 */
#define REJECT_HOLD_MS            5000U   /* 84E 判为非人体的位置在此时间内不再送识别 */
#define REJECT_ANGLE_DEG          10.0f
#define REJECT_DIST_CM            50.0f

typedef struct {
    uint8_t active;
    uint8_t target_id;
    float angle;        /* 目标方位角（°） */
    float distance;     /* cm */
    float width;        /* 仅84E：扫描窗口宽度（°） */
    float margin;       /* 仅84E：距离窗口半宽（cm） */
    uint8_t predicted;  /* 仅84E：方位/窗宽是否来自预判 */
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

/* 每个目标的时间线与最近观测（按 target id-1 索引） */
typedef struct {
    uint32_t discover_tick;     /* 雷达首次发现 */
    uint32_t lost_tick;         /* 最近一次进入丢失 */
    uint32_t hist_tick;         /* 最近一次写入预判历史 */
    uint32_t fuse_log_tick;
    uint8_t  lock_logged;       /* 已记录首次锁定 */
    float    primary_bearing;   /* 主板最近一次图像修正后的方位 */
    uint32_t primary_tick;
    uint8_t  primary_esp;
} TargetTrace_t;
static TargetTrace_t g_trace[TARGET_MAX_COUNT];

static TargetTrace_t *Trace(uint8_t id)
{
    return (id >= 1U && id <= TARGET_MAX_COUNT) ? &g_trace[id - 1U] : NULL;
}

/* 84E 判为非人体的位置，短时间内不再重复送识别 */
typedef struct {
    float angle;
    float dist;
    uint32_t tick;
} RejectEntry_t;
#define REJECT_SLOTS  4U
static RejectEntry_t g_reject[REJECT_SLOTS];
static uint8_t g_rejectNext = 0U;

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

/* ──── 方位角 ↔ 云台 Pan（考虑安装偏置） ──── */
static const float s_panOffset[GIMBAL_COUNT] = {
    MOUNT_CAM0_PAN_OFFSET, MOUNT_CAM1_PAN_OFFSET, MOUNT_CAM2_PAN_OFFSET
};

static float BearingToPanRaw(GimbalId_t gimbal, float bearing_deg)
{
    return GIMBAL_PAN_INITIAL_ANGLE + bearing_deg - s_panOffset[gimbal];
}

static float BearingToPanAngle(GimbalId_t gimbal, float bearing_deg)
{
    float angle = BearingToPanRaw(gimbal, bearing_deg);
    if (angle < GIMBAL_PAN_HARD_MIN) angle = GIMBAL_PAN_HARD_MIN;
    if (angle > GIMBAL_PAN_HARD_MAX) angle = GIMBAL_PAN_HARD_MAX;
    return angle;
}

/* 云台当前指向的方位角 */
static float GimbalBearing(GimbalId_t gimbal)
{
    return Gimbal_GetEstimatedPan(gimbal) - GIMBAL_PAN_INITIAL_ANGLE + s_panOffset[gimbal];
}

/* 该 ESP32 云台能否在接力边界以内对准该方位 */
static uint8_t EspCovers(uint8_t esp, float bearing_deg)
{
    float pan = BearingToPanRaw((GimbalId_t)esp, bearing_deg);
    return (pan >= GIMBAL_PAN_HANDOVER_LOW + COVER_MARGIN_DEG &&
            pan <= GIMBAL_PAN_HANDOVER_HIGH - COVER_MARGIN_DEG) ? 1U : 0U;
}

/* 估计云台从当前位置转到某方位所需时间（s） */
static float PanTravelTime(GimbalId_t gimbal, float bearing_deg)
{
    float delta = BearingToPanAngle(gimbal, bearing_deg) - Gimbal_GetEstimatedPan(gimbal);
    if (delta < 0.0f) delta = -delta;
    return delta * ((float)GIMBAL_PAN_135_MOVE_MS / 135.0f) / 1000.0f;
}

/* ──── ESP32 角色管理 ──── */
static void EspAssign(uint8_t esp, EspRole_t role, uint8_t target_id)
{
    if (esp > 1U) return;
    g_espRole[esp] = role;
    g_espTarget[esp] = target_id;
    ExpLog_Event("ASSIGN", target_id, esp, (int32_t)role, 0);
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
    Gimbal_MovePanTo((GimbalId_t)esp, BearingToPanAngle((GimbalId_t)esp, gimbal_bearing));
}

/* ──── 84E 请求：空闲则预转云台后识别，忙则排队 ──── */
static void Dispatch84E(const PendingAim_t *aim)
{
    g_pending84E = *aim;
    g_pending84E.active = 1U;
    g_pending84E.predicted = 0U;

#if EXP_PREDICTIVE_AIM
    /* 已有历史的目标：外推到“云台转到位 + 推理完成”时刻的方位，
     * 并按方位波动放宽扫描窗。预判两次以收敛转动时间估计。 */
    float pb, pd, spread;
    float horizon = PanTravelTime(GIMBAL_CAM2, aim->angle) + RECOG_LATENCY_S;
    if (MotionPredict_Get(aim->target_id, horizon, &pb, &pd, &spread)) {
        horizon = PanTravelTime(GIMBAL_CAM2, pb) + RECOG_LATENCY_S;
        MotionPredict_Get(aim->target_id, horizon, &pb, &pd, &spread);
        float width = aim->width + 2.0f * spread;
        if (width > MAX_84E_WIDTH_DEG) width = MAX_84E_WIDTH_DEG;
        g_pending84E.angle = pb;
        g_pending84E.distance = pd;
        g_pending84E.width = width;
        g_pending84E.predicted = 1U;
    }
#endif

    g_84EBusy = 1U;
    g_84ESentTick = 0U;
    g_active84ETargetId = aim->target_id;
    Gimbal_MovePanTo(GIMBAL_CAM2, BearingToPanAngle(GIMBAL_CAM2, g_pending84E.angle));
    g_currentProc = PROC_84E_RECOG;
}

static void Request84E(uint8_t target_id, float angle, float distance,
                       float width, float margin)
{
    PendingAim_t aim = {1U, target_id, angle, distance, width, margin, 0U};
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

/* ──── 84E 判为非人体的位置记录 ──── */
static void Reject_Add(float angle, float dist)
{
    g_reject[g_rejectNext].angle = angle;
    g_reject[g_rejectNext].dist = dist;
    g_reject[g_rejectNext].tick = HAL_GetTick();
    if (g_reject[g_rejectNext].tick == 0U) g_reject[g_rejectNext].tick = 1U;
    g_rejectNext = (uint8_t)((g_rejectNext + 1U) % REJECT_SLOTS);
}

static uint8_t Reject_Match(float angle, float dist)
{
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < REJECT_SLOTS; i++) {
        if (g_reject[i].tick == 0U || (now - g_reject[i].tick) > REJECT_HOLD_MS) continue;
        if (fabsf(g_reject[i].angle - angle) < REJECT_ANGLE_DEG &&
            fabsf(g_reject[i].dist - dist) < REJECT_DIST_CM) return 1U;
    }
    return 0U;
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
    MotionPredict_Init();
    DataFusion_Init(&g_fusion);
    memset(g_trace, 0, sizeof(g_trace));

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
    Logger_Print(LOG_INFO, "Scheduler Ready. predictive=%d assist=%d",
                 EXP_PREDICTIVE_AIM, EXP_ASSIST_ENABLE);
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
        ExpLog_Event("REQ84E", g_pending84E.target_id,
                     (int32_t)(g_pending84E.angle * 10.0f),
                     (int32_t)(g_pending84E.width * 10.0f),
                     g_pending84E.predicted);
        g_84ESentTick = HAL_GetTick();
        if (g_84ESentTick == 0U) g_84ESentTick = 1U;
        g_pending84E.active = 0U;
    }

    /* 84E 无回复：解除忙状态，继续下一候选或回到雷达扫描 */
    if (g_84EBusy && !g_pending84E.active && g_84ESentTick != 0U &&
        (HAL_GetTick() - g_84ESentTick) >= RECOG_TIMEOUT_MS) {
        Logger_Print(LOG_WARN, "84E recog timeout");
        ExpLog_Event("TO84E", g_active84ETargetId, 0, 0, 0);
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

/* 释放某目标占用的全部 ESP32 并移出列表 */
static void ReleaseTarget(uint8_t id, int32_t reason)
{
    for (uint8_t i = 0; i < 2; i++) {
        if (g_espRole[i] != ESP_ROLE_IDLE && g_espTarget[i] == id) {
            EspRelease(i);
        }
    }
    if (g_handover.active && g_handover.target_id == id) {
        g_handover.active = 0U;
    }
    ExpLog_Event("RELEASE", id, reason, 0, 0);
    TargetList_Remove(&g_targetList, id);
    RelockLogic_ResetRetries(id);
    MotionPredict_Reset(id);
    DataFusion_Remove(&g_fusion, id);
    TargetTrace_t *tr = Trace(id);
    if (tr) memset(tr, 0, sizeof(*tr));
    g_currentProc = PROC_IDLE;
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
            if (g_candidates[i].confidence <= 0.3f) continue;
            if (Reject_Match(g_candidates[i].angle_deg, g_candidates[i].distance_cm)) continue;

            int8_t idx = TargetList_FindOrAdd(&g_targetList, &g_candidates[i]);
            if (idx < 0) continue;
            TrackedTarget_t *t = &g_targetList.targets[idx];
            TargetTrace_t *tr = Trace(t->id);
            if (tr && tr->discover_tick == 0U) {
                tr->discover_tick = now;
                ExpLog_Event("CAND", t->id, (int32_t)(t->angle_deg * 10.0f),
                             (int32_t)t->distance_cm, 0);
            }
            if (t->state == TARGET_STATE_TRACKED) continue;

            MotionPredict_Update(t->id, t->angle_deg, t->distance_cm);
            /* 84E空闲则立即预转识别，忙则入队等空闲后自动发送 */
            Request84E(t->id, t->angle_deg, t->distance_cm,
                       DEFAULT_84E_WIDTH_DEG, DEFAULT_84E_MARGIN_CM);
        }
    }

    if (!g_pending84E.active) {
        g_currentProc = PROC_IDLE;
    }
}

static void On84EResult(uint8_t id, float conf, float x, float y, float dist, float angle)
{
    (void)x; (void)y; (void)dist; (void)angle;   /* 84E 不测距，位置以 H7 侧为准 */
    uint32_t latency = (g_84ESentTick != 0U) ? (HAL_GetTick() - g_84ESentTick) : 0U;
    g_84EBusy = 0U;
    g_84ESentTick = 0U;
    g_active84ETargetId = 0U;
    DelayRecord_Stop84E(id);
    ExpLog_Event("RES84E", id, (int32_t)(conf * 1000.0f), (int32_t)latency, 0);

    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);

    if (conf <= 0.3f) {
        /* 识别为非人体：从未跟踪过的候选直接移除，并短时屏蔽该位置 */
        if (t && t->state != TARGET_STATE_TRACKED && t->state != TARGET_STATE_LOST) {
            Reject_Add(t->angle_deg, t->distance_cm);
            ReleaseTarget(id, 0);
        }
        if (!Dispatch84EFromQueue()) {
            g_currentProc = PROC_IDLE;
        }
        return;
    }

    TargetState_Update(&g_targetList, id, TARGET_STATE_TRACKED);
    if (t) {
        DataFusion_UpdateWith84E(&g_fusion, id, conf, GimbalBearing(GIMBAL_CAM2),
                                 t->distance_cm, HAL_GetTick());
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

static void OnPrimaryResult(uint8_t esp_id, TrackedTarget_t *t,
                            float x, float y,
                            float pan_ctrl, float tilt_ctrl, uint8_t lost)
{
    uint32_t now = HAL_GetTick();
    TargetTrace_t *tr = Trace(t->id);
    (void)y;

    if (lost) {
        Gimbal_SetPanMotion((GimbalId_t)esp_id, GIMBAL_PAN_STOP);
        if (t->state != TARGET_STATE_LOST) {
            TargetState_Update(&g_targetList, t->id, TARGET_STATE_LOST);
            g_lastRelockAttempt[esp_id] = 0U;
            if (tr) tr->lost_tick = now;
            ExpLog_Event("LOST", t->id, esp_id, 0, 0);
        }
        if (g_lastRelockAttempt[esp_id] != 0U &&
            (now - g_lastRelockAttempt[esp_id]) < RELOCK_INTERVAL_MS) {
            return;
        }
        g_lastRelockAttempt[esp_id] = now;

        RelockPlan_t plan;
        if (RelockLogic_Start(t, &plan) == RELOCK_RELEASE) {
            ReleaseTarget(t->id, 1);
            return;
        }
#if EXP_PREDICTIVE_AIM
        /* 有足够历史时用卡尔曼外推代替简单三帧外推 */
        {
            float pb, pd, spread;
            float horizon = (tr && tr->lost_tick) ? (float)(now - tr->lost_tick) / 1000.0f : 0.0f;
            horizon += PanTravelTime((GimbalId_t)esp_id, plan.pred_angle);
            if (MotionPredict_Get(t->id, horizon, &pb, &pd, &spread)) {
                plan.pred_angle = pb;
                plan.pred_dist = pd;
                plan.scan_width += 2.0f * spread;
                if (plan.scan_width > MAX_84E_WIDTH_DEG) plan.scan_width = MAX_84E_WIDTH_DEG;
            }
        }
#endif
        /* 经调度器请求84E（预转云台、忙则排队），只让本目标的主板转向预测方位复锁 */
        Request84E(t->id, plan.pred_angle, plan.pred_dist,
                   plan.scan_width, plan.dist_margin);
        AimESP(esp_id, t->id, plan.pred_angle, plan.pred_angle, plan.pred_dist);
        return;
    }

    if (t->state != TARGET_STATE_TRACKED) {
        if (t->state == TARGET_STATE_LOST && tr && tr->lost_tick) {
            ExpLog_Event("RELOCK", t->id, esp_id, (int32_t)(now - tr->lost_tick), 0);
            tr->lost_tick = 0U;
        }
        TargetState_Update(&g_targetList, t->id, TARGET_STATE_TRACKED);
    }
    RelockLogic_ResetRetries(t->id);
    g_lastRelockAttempt[esp_id] = 0U;

    if (tr && !tr->lock_logged) {
        tr->lock_logged = 1U;
        ExpLog_Event("LOCK", t->id, esp_id,
                     tr->discover_tick ? (int32_t)(now - tr->discover_tick) : -1, 0);
    }

    Gimbal_ApplyTrackingControl((GimbalId_t)esp_id, pan_ctrl, tilt_ctrl);
    /* H7 统一控制云台：目标方位 = 主板云台指向 + 图像水平偏差 */
    float bearing = DataFusion_ESP32Bearing(GimbalBearing((GimbalId_t)esp_id), x);
    t->angle_deg = bearing;
    t->x = x;
    t->y = y;
    t->esp_assigned = esp_id;
    DataFusion_UpdateWithESP32(&g_fusion, t->id, esp_id, bearing, t->distance_cm, now);
    if (tr) {
        tr->primary_bearing = bearing;
        tr->primary_tick = now;
        tr->primary_esp = esp_id;
        if ((now - tr->hist_tick) >= HISTORY_INTERVAL_MS) {
            tr->hist_tick = now;
            RelockLogic_RecordPosition(t->id, bearing, t->distance_cm);
            MotionPredict_Update(t->id, bearing, t->distance_cm);
        }
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

/* 辅助板观测：与主板最近观测组成两条方位射线做三角定位 */
static void OnAssistResult(uint8_t esp_id, TrackedTarget_t *t,
                           float x, float pan_ctrl, float tilt_ctrl, uint8_t lost)
{
    uint32_t now = HAL_GetTick();
    if (lost) {
        Gimbal_SetPanMotion((GimbalId_t)esp_id, GIMBAL_PAN_STOP);
        return;
    }
    Gimbal_ApplyTrackingControl((GimbalId_t)esp_id, pan_ctrl, tilt_ctrl);

    TargetTrace_t *tr = Trace(t->id);
    if (!tr || tr->primary_tick == 0U || tr->primary_esp == esp_id ||
        (now - tr->primary_tick) > TRIANGULATE_MAX_SKEW_MS) return;

    float assist_bearing = DataFusion_ESP32Bearing(GimbalBearing((GimbalId_t)esp_id), x);
    float ba = (esp_id == 0U) ? assist_bearing : tr->primary_bearing;   /* ESP32-A 的射线 */
    float bb = (esp_id == 0U) ? tr->primary_bearing : assist_bearing;   /* ESP32-B 的射线 */
    float tx, ty;
    if (!Geometry_Triangulate(DEVICE_TRACK_A_X_CM, DEVICE_TRACK_A_Y_CM, ba,
                              DEVICE_TRACK_B_X_CM, DEVICE_TRACK_B_Y_CM, bb,
                              &tx, &ty)) return;

    DataFusion_UpdateTriangulated(&g_fusion, t->id, tx, ty, now);
    float tri_bearing, tri_dist;
    Geometry_PlaneToPolar(DEVICE_RADAR_X_CM, DEVICE_RADAR_Y_CM, tx, ty,
                          &tri_bearing, &tri_dist);
    (void)tri_bearing;
    ExpLog_Event("TRI", t->id, (int32_t)tx, (int32_t)ty,
                 (int32_t)(tri_dist - t->distance_cm));
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
    g_espFrames[esp_id]++;
    if (lost) g_espLostFrames[esp_id]++;

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
            ExpLog_Event("HOOK", id, esp_id,
                         (int32_t)(now - g_handover.start_tick), 0);
            EspRelease(g_handover.from_esp);
            EspAssign(esp_id, ESP_ROLE_TRACK, id);
            g_handover.active = 0U;
            OnPrimaryResult(esp_id, t, x, y, pan_ctrl, tilt_ctrl, lost);
        }
        break;

    case ESP_ROLE_ASSIST:
        OnAssistResult(esp_id, t, x, pan_ctrl, tilt_ctrl, lost);
        break;

    default:
        break;
    }
}

static void StartHandover(uint8_t from_esp, TrackedTarget_t *t)
{
    if (!t || from_esp > 1U || g_handover.active) return;

    uint8_t to_esp = (uint8_t)(1U - from_esp);
    /* 接手板正在跟踪另一个目标，或其云台对不准该方位时不接力 */
    if (g_espRole[to_esp] == ESP_ROLE_TRACK && g_espTarget[to_esp] != t->id) return;
    float bearing = t->angle_deg;
    if (!EspCovers(to_esp, bearing)) return;

    g_handover.active = 1U;
    g_handoverArmed = 0U;
    g_handover.target_id = t->id;
    g_handover.from_esp = from_esp;
    g_handover.to_esp = to_esp;
    g_handover.start_tick = HAL_GetTick();
    ExpLog_Event("HOSTART", t->id, from_esp, to_esp,
                 (int32_t)(Gimbal_GetEstimatedPan((GimbalId_t)from_esp) * 10.0f));

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
    ExpLog_Event("HOFAIL", g_handover.target_id, to_esp, 0, 0);
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
     * 首选板的云台对不准该方位时改用另一块（两块板安装朝向不同）。
     */
    uint8_t esp_id;
    if (g_targetList.count <= 1) {
        esp_id = 0U;
    } else {
        uint8_t idx = (uint8_t)(t - g_targetList.targets);
        esp_id = (uint8_t)(idx % 2U);
    }

    /* 主跟踪/接手中的板视为占用；辅助板可以被改派。两块都忙时由H7保留目标。 */
    uint8_t other = (uint8_t)(1U - esp_id);
    uint8_t busy_pref  = (g_espRole[esp_id] == ESP_ROLE_TRACK || g_espRole[esp_id] == ESP_ROLE_HANDOVER);
    uint8_t busy_other = (g_espRole[other]  == ESP_ROLE_TRACK || g_espRole[other]  == ESP_ROLE_HANDOVER);
    if (busy_pref || (!EspCovers(esp_id, t->angle_deg) &&
                      !busy_other && EspCovers(other, t->angle_deg))) {
        esp_id = other;
    }
    if (g_espRole[esp_id] == ESP_ROLE_TRACK ||
        g_espRole[esp_id] == ESP_ROLE_HANDOVER) return;

    EspAssign(esp_id, ESP_ROLE_TRACK, t->id);
    t->esp_assigned = esp_id;
    TargetTrace_t *tr = Trace(t->id);
    if (tr) tr->lock_logged = 0U;
    AimESP(esp_id, t->id, t->angle_deg, t->angle_deg, t->distance_cm);
}

static void Process_CheckClosedLoop(void)
{
    static uint32_t last_check_tick = 0;
    static uint32_t last_fps_tick = 0;
    uint32_t now = HAL_GetTick();
    if ((now - last_check_tick) < 100U) return;
    last_check_tick = now;

    /* ESP32 回传帧率（实验指标：视觉跟踪帧率） */
    if ((now - last_fps_tick) >= 1000U) {
        last_fps_tick = now;
        for (uint8_t i = 0; i < 2; i++) {
            if (g_espFrames[i]) {
                ExpLog_Event("FPS", g_espTarget[i], i, g_espFrames[i], g_espLostFrames[i]);
            }
            g_espFrames[i] = 0U;
            g_espLostFrames[i] = 0U;
        }
    }

    /* 辅助板的目标已不存在 → 释放 */
    for (uint8_t i = 0; i < 2; i++) {
        if (g_espRole[i] == ESP_ROLE_ASSIST &&
            !TargetList_GetTarget(&g_targetList, g_espTarget[i])) {
            EspRelease(i);
        }
    }

    for (int i = 0; i < g_targetList.count; i++) {
        TrackedTarget_t *t = &g_targetList.targets[i];

        TargetTrace_t *tr = Trace(t->id);
        const FusedTarget_t *ft = DataFusion_Get(&g_fusion, t->id);
        if (tr && ft && (now - tr->fuse_log_tick) >= 500U) {
            tr->fuse_log_tick = now;
            ExpLog_Event("FUSE", t->id, (int32_t)ft->x, (int32_t)ft->y, ft->updated_by);
        }

        if (t->state != TARGET_STATE_TRACKED || t->esp_assigned > 1U) continue;
        uint8_t p = t->esp_assigned;
        if (g_espRole[p] != ESP_ROLE_TRACK || g_espTarget[p] != t->id) continue;

        /* 链路看门狗：只在主板长时间无回传时限频重发一次 */
        if (!g_pendingESP[p].active &&
            ClosedLoop_NeedsResend(t, now - g_espLastFrame[p],
                                   now - g_espLastResend[p])) {
            AimESP(p, t->id, t->angle_deg, t->angle_deg, t->distance_cm);
        }

#if EXP_ASSIST_ENABLE
        /* 单目标：另一块空闲且能覆盖该方位的板启动一次辅助观察（不重复下发） */
        uint8_t other = (uint8_t)(1U - p);
        float assist_bearing = AssistLocalize_ComputeBearing(t);
        if (g_targetList.count == 1 && !g_handover.active &&
            g_espRole[other] == ESP_ROLE_IDLE && !g_pendingESP[other].active &&
            EspCovers(other, assist_bearing)) {
            EspAssign(other, ESP_ROLE_ASSIST, t->id);
            AimESP(other, t->id, t->angle_deg, assist_bearing, t->distance_cm);
        }
#endif
    }
}
