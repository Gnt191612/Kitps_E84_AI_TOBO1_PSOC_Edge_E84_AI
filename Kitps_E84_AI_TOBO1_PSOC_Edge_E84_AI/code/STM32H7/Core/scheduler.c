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
#include "radar_map.h"
#include "target_candidate.h"
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
#include "console.h"
#include "delay_record.h"
#include "error_rate.h"
#include <string.h>
#include <math.h>

static TargetList_t    g_targetList;
static DataFusion_t    g_fusion;

/* g_currentProc 只反映当前阶段（日志/调试用）；雷达扫描在每轮主循环都运行，
 * 不受其限制——否则锁定第一个目标后就再也发现不了新目标。 */
static Process_t       g_currentProc = PROC_RADAR_SCAN;

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
static uint8_t         g_espResends[2] = {0, 0};     /* 上次回传以来的恢复性重发次数 */
static uint8_t         g_espSilent[2] = {0, 0};      /* 曾因无回传被换下，收到任意一帧或 STOP 后清除 */
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
/* 人体判定阈值：必须与 84E 的 HUMAN_CONF_THRESHOLD（inference_task.h）相同 */
#define RECOG_HUMAN_CONF          0.5f
#define RADAR_CAND_MIN_CONF       0.3f    /* 雷达候选的最低置信度，与人体判定无关 */
#define COMM_REPORT_MS            5000U   /* 通信统计事件的输出间隔 */
#define RECOG_LATENCY_S           0.2f    /* 预判外推时计入的 84E 推理与通信时间 */
#define COVER_MARGIN_DEG          10.0f   /* 判断云台能否覆盖某方位时留的余量 */
#define TRIANGULATE_MAX_SKEW_MS   150U    /* 两路观测时间差上限 */
#define REJECT_HOLD_MS            5000U   /* 84E 判为非人体的位置在此时间内不再送识别 */
#define REJECT_ANGLE_DEG          10.0f
#define REJECT_DIST_CM            50.0f
#define RADAR_SAME_TARGET_DEG     20.0f   /* 雷达候选与已跟踪目标视为同一人的最小范围 */
#define RADAR_SAME_TARGET_BEAM_DEG 15.0f  /* 波束展宽造成的方位偏差 */
#define RADAR_SAME_TARGET_BODY_CM 40.0f   /* 人体宽度：越近占的角度越大，雷达报的中心偏得越多 */
#define RADAR_SAME_TARGET_CM      80.0f   /* 刚更新过距离时的范围，之后按走动速度放宽 */
#define RADAR_SAME_TARGET_MAX_CM  250.0f
#define TARGET_WALK_CM_PER_S      100.0f  /* 按此速度估计两次测距之间可能走动的距离 */
#define TRI_DIST_BLEND            0.2f    /* 三角定位距离更新目标距离的系数 */
#define REJECT_CONFIRM_COUNT      3U      /* 连续判为非人体这么多次才并入背景，防止一次漏判把人学成背景 */
#define STALE_CANDIDATE_MS        15000U  /* 未识别候选超过该时间雷达未再报出则移除（约一个扫描周期多） */
#define ASSIST_REAIM_MS           1000U   /* 辅助板看不到目标时重新瞄准的间隔 */
#define TRACK_ACQUIRE_MS          500U    /* 跟踪板收到命令后这段时间还在找目标，没看到不算跟丢 */
#define SCAN_CONFIRM_FRAMES       3U      /* 84E 连续这么多帧判为人才认定，单帧误报不算 */
#define SCAN_REJECT_FRAMES        10U     /* 累计这么多帧判为非人体就放弃 */
#define SCAN_MAX_MS               3000U   /* 一次边扫描边跟踪的时长上限 */
#define SCAN_PAN_GAIN             0.6f    /* 每帧把画面偏差修正多少 */
#define SCAN_PAN_STEP_MAX         6.0f    /* 每帧最多转多少度 */
#define SCAN_TILT_GAIN            120.0f  /* 竖直偏差（°）换算成云台控制量 */
#define SCAN_MAX_VEL_DEG_S        60.0f   /* 估算目标角速度的上限 */
#define SCAN_SAME_TARGET_DEG      10.0f   /* 84E 盯的方向离已跟踪目标这么近，就是同一个人 */
#define E84_IMG_WIDTH_PX          160.0f  /* 84E 模型输入分辨率 */
#define E84_IMG_HEIGHT_PX         120.0f
#define E84_CAM_FOV_H_DEG         50.0f   /* OV7675 视场未查证，先按和 OV2640 相同 */
#define E84_CAM_FOV_V_DEG         38.0f

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

#if EXP_RECOG_WHILE_TRACK
/* 84E 边扫描边跟踪的进度 */
typedef struct {
    uint8_t  active;
    uint8_t  consec_human;  /* 连续判为人的帧数 */
    uint8_t  low_frames;    /* 累计判为非人体的帧数 */
    uint8_t  frames;
    float    conf_sum;
    uint32_t start_tick;
    uint8_t  has_bearing;   /* 84E 已经在画面里找到目标 */
    float    bearing;       /* 目标相对 84E 的方位 */
    float    vel_deg_s;     /* 方位变化速度，交给 ESP32 时外推用 */
    uint32_t bearing_tick;
} ScanTrack_t;
static ScanTrack_t g_scan = {0};
/* 非空时 Process_InvokeESP32 按 84E 给的坐标瞄准（见 ScanAimBearing） */
static const ScanTrack_t *g_scanHandoff = NULL;
#endif

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
    uint32_t radar_tick;        /* 雷达最近一次报出该目标 */
    uint32_t lost_tick;         /* 最近一次进入丢失 */
    uint32_t hist_tick;         /* 最近一次写入预判历史 */
    uint32_t fuse_log_tick;
    uint8_t  lock_logged;       /* 已记录首次锁定 */
    uint8_t  third_reported;    /* 已作为“无空闲跟踪板”的目标上报 */
    float    primary_bearing;   /* 主板最近一次图像修正后的方位 */
    uint32_t primary_tick;
    uint8_t  primary_esp;
    uint8_t  reject_count;      /* 连续被 84E 判为非人体的次数 */
    uint32_t dist_tick;         /* 目标距离最近一次更新 */
    float    radar_dist;        /* 雷达最近一次测到的距离 */
    float    radar_angle;       /* 雷达最近一次报的方位。84E 跟随后 angle_deg 会变成物体中心，并入背景要按雷达报的来 */
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
static void ReleaseTarget(uint8_t id, int32_t reason);
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

/* 跟踪目标的方位是主板看到的，换算成从 (ox, oy) 看到的方位。
 * 各观测点相距 20~40 cm，人在 1~2 m 内时方位能差 10~20°，不能混用。 */
static float BearingFromPoint(const TrackedTarget_t *t, float ox_cm, float oy_cm)
{
    if (t->esp_assigned > 1U) return t->angle_deg;
    float x, y, bearing, dist;
    Geometry_PolarToPlane((t->esp_assigned == 0U) ? DEVICE_TRACK_A_X_CM : DEVICE_TRACK_B_X_CM,
                          (t->esp_assigned == 0U) ? DEVICE_TRACK_A_Y_CM : DEVICE_TRACK_B_Y_CM,
                          t->angle_deg, t->distance_cm, &x, &y);
    Geometry_PlaneToPolar(ox_cm, oy_cm, x, y, &bearing, &dist);
    return bearing;
}

static float BearingFromBoard(const TrackedTarget_t *t, uint8_t esp)
{
    return BearingFromPoint(t, (esp == 0U) ? DEVICE_TRACK_A_X_CM : DEVICE_TRACK_B_X_CM,
                            (esp == 0U) ? DEVICE_TRACK_A_Y_CM : DEVICE_TRACK_B_Y_CM);
}

#if EXP_RECOG_WHILE_TRACK
/* 84E 给的坐标换算成 esp 这块板要瞄的方位，并外推到它转到位的时刻：
 * 人在走，瞄现在的位置，转过去时人已经走开了 */
static float ScanAimBearing(const ScanTrack_t *scan, const TrackedTarget_t *t, uint8_t esp)
{
    float ox = (esp == 0U) ? DEVICE_TRACK_A_X_CM : DEVICE_TRACK_B_X_CM;
    float oy = (esp == 0U) ? DEVICE_TRACK_A_Y_CM : DEVICE_TRACK_B_Y_CM;
    float x, y, bearing = 0.0f, dist;
    float lead = 0.0f;
    for (int i = 0; i < 2; i++) {   /* 转动时间取决于方位，算两次收敛 */
        Geometry_PolarToPlane(DEVICE_RECOG_X_CM, DEVICE_RECOG_Y_CM,
                              scan->bearing + scan->vel_deg_s * lead, t->distance_cm, &x, &y);
        Geometry_PlaneToPolar(ox, oy, x, y, &bearing, &dist);
        lead = PanTravelTime((GimbalId_t)esp, bearing);
    }
    return bearing;
}
#endif

/* 84E 能否对准该方位：转到背后时看不到人，不能拿来判断是不是人 */
static uint8_t Cam84ECovers(float bearing_deg)
{
    float pan = BearingToPanRaw(GIMBAL_CAM2, bearing_deg);
    return (pan >= GIMBAL_PAN_HARD_MIN + COVER_MARGIN_DEG &&
            pan <= GIMBAL_PAN_HARD_MAX - COVER_MARGIN_DEG) ? 1U : 0U;
}

/* 是否有 ESP32 正在（或正接手）跟踪该目标 */
static uint8_t HasTracker(uint8_t target_id)
{
    for (uint8_t i = 0; i < 2; i++) {
        if ((g_espRole[i] == ESP_ROLE_TRACK || g_espRole[i] == ESP_ROLE_HANDOVER) &&
            g_espTarget[i] == target_id) return 1U;
    }
    return 0U;
}

/* ──── ESP32 角色管理 ──── */
static void EspAssign(uint8_t esp, EspRole_t role, uint8_t target_id)
{
    if (esp > 1U) return;
    g_espRole[esp] = role;
    g_espTarget[esp] = target_id;
    g_espResends[esp] = 0U;
    ExpLog_Event("ASSIGN", target_id, esp, (int32_t)role, 0);
}

static void EspRelease(uint8_t esp)
{
    if (esp > 1U) return;
    if (g_espRole[esp] != ESP_ROLE_IDLE) ExpLog_Event("ESPFREE", g_espTarget[esp], esp, 0, 0);
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
    Console_Init();

    /* 先保持135°初始朝向，再进入左侧扫描段。 */
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    g_sweepPhaseStart = HAL_GetTick();

    g_currentProc  = PROC_RADAR_SCAN;
    Logger_Print(LOG_INFO, "Scheduler Ready. predictive=%d assist=%d",
                 EXP_PREDICTIVE_AIM, EXP_ASSIST_ENABLE);
}

void Scheduler_Run(void)
{
    Console_Poll();               /* 调试串口 STOP/START/STATUS 命令 */
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

    Process_Radar();
    Process_CheckClosedLoop();
}

void Scheduler_RequestShutdown(void)
{
    if (g_shutdownRequested) return;

    ExpLog_Event("PAUSE", 0, 0, 0, 0);
    g_shutdownRequested = 1U;
    g_shutdownComplete = 0U;
    g_pending84E.active = 0U;
    g_handover.active = 0U;
#if EXP_RECOG_WHILE_TRACK
    if (g_scan.active) Cmd_84E_SendStopCmd(g_active84ETargetId);
    g_scan.active = 0U;
#endif
    g_active84ETargetId = 0U;
    g_84EBusy = 0U;
    g_84ESentTick = 0U;
    memset(&g_recogQueue, 0, sizeof(g_recogQueue));

    /* 清空全部目标（同时释放其占用的 ESP32），恢复后从头发现 */
    while (g_targetList.count > 0) {
        ReleaseTarget(g_targetList.targets[0].id, 3);
    }
    EspRelease(0);
    EspRelease(1);
    g_espSilent[0] = g_espSilent[1] = 0U;
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

uint8_t Scheduler_IsPaused(void)
{
    return g_shutdownRequested;
}

void Scheduler_Resume(void)
{
    if (!g_shutdownRequested) return;
    g_shutdownRequested = 0U;
    g_shutdownComplete = 0U;
    g_radarHoming = 0U;
    g_handoverArmed = 1U;
    g_sweepPhase = SWEEP_LEFT_OUT;
    g_sweepPhaseStart = HAL_GetTick();
    g_currentProc = PROC_RADAR_SCAN;
    Logger_Print(LOG_INFO, "Scheduler resumed");
    ExpLog_Event("RESUME", 0, 0, 0, 0);
}

void Scheduler_RelearnBackground(void)
{
    /* 借用停止流程清空目标和队列，云台回初始朝向，然后立即恢复扫描 */
    Scheduler_RequestShutdown();
    memset(g_reject, 0, sizeof(g_reject));
    Radar_Process_Relearn();
    ExpLog_Event("RELEARN", 0, 0, 0, 0);
    Scheduler_Resume();
}

void Scheduler_PrintStatus(void)
{
    static const char *role_name[] = {"idle", "track", "assist", "handover"};
    Logger_Print(LOG_INFO, "STATUS paused=%u radar_bg=%u targets=%u 84E_busy=%u queue=%u",
                 g_shutdownRequested, Radar_Process_BackgroundReady(),
                 g_targetList.count, g_84EBusy, g_recogQueue.count);
    for (uint8_t i = 0; i < 2; i++) {
        Logger_Print(LOG_INFO, "  ESP32-%c role=%s target=%u silent=%u",
                     (i == 0) ? 'A' : 'B', role_name[g_espRole[i]], g_espTarget[i], g_espSilent[i]);
    }
    for (int i = 0; i < g_targetList.count; i++) {
        const TrackedTarget_t *t = &g_targetList.targets[i];
        Logger_Print(LOG_INFO, "  target %u state=%u bearing=%.1f dist=%.0f esp=%u",
                     t->id, t->state, t->angle_deg, t->distance_cm, t->esp_assigned);
    }
    static const char *ch_name[PROTO_CH_COUNT] = {"84E", "ESP32-A", "ESP32-B"};
    for (uint8_t ch = 0; ch < PROTO_CH_COUNT; ch++) {
        Protocol_CommStats_t s;
        Protocol_GetCommStats(ch, &s);
        Logger_Print(LOG_INFO, "  link %s frames=%lu xor=%lu bad_len=%lu drops=%lu uart=%lu",
                     ch_name[ch], (unsigned long)s.frames, (unsigned long)s.xor_errors,
                     (unsigned long)s.bad_len, (unsigned long)s.queue_drops,
                     (unsigned long)s.uart_errors);
    }
}

static void RadarSweep_Update(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - g_sweepPhaseStart;
    /* 不进入两端的忽略范围，扫描角速度不变 */
    const float span = 135.0f - RADAR_EDGE_IGNORE_DEG;
    const uint32_t leg_ms = (uint32_t)((float)SERVO_SWEEP_LEG_MS * span / 135.0f);
    if (elapsed > leg_ms) elapsed = leg_ms;
    float progress = (float)elapsed / (float)leg_ms;
    float angle = SERVO_ANGLE_CENTER;

    switch (g_sweepPhase) {
    case SWEEP_LEFT_OUT:     angle = SERVO_ANGLE_CENTER - span * progress; break;
    case SWEEP_LEFT_RETURN:  angle = SERVO_ANGLE_CENTER - span * (1.0f - progress); break;
    case SWEEP_RIGHT_OUT:    angle = SERVO_ANGLE_CENTER + span * progress; break;
    case SWEEP_RIGHT_RETURN: angle = SERVO_ANGLE_CENTER + span * (1.0f - progress); break;
    }
    Servo_SetAngle(angle);

    if (elapsed < leg_ms) return;

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
#if EXP_RECOG_WHILE_TRACK
        memset(&g_scan, 0, sizeof(g_scan));
        g_scan.active = 1U;
        g_scan.start_tick = HAL_GetTick();
        Cmd_84E_SendScanTrackCmd(g_pending84E.target_id,
                                 g_pending84E.angle, g_pending84E.width,
                                 g_pending84E.distance - g_pending84E.margin,
                                 g_pending84E.distance + g_pending84E.margin);
#else
        Cmd_84E_SendScanCmd(g_pending84E.target_id,
                            g_pending84E.angle, g_pending84E.width,
                            g_pending84E.distance - g_pending84E.margin,
                            g_pending84E.distance + g_pending84E.margin);
#endif
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
#if EXP_RECOG_WHILE_TRACK
        if (g_scan.active) Cmd_84E_SendStopCmd(g_active84ETargetId);
        g_scan.active = 0U;
#endif
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

/* 雷达采集与目标检测：非阻塞，每轮主循环调用。
 * 只有“比背景明显变近”的位置才会产生候选（见 Radar/radar_map.h）。 */
static void Process_Radar(void)
{
    TargetCandidate_t cand;
    if (!Radar_Process_Update(&cand)) return;
    if (cand.confidence <= RADAR_CAND_MIN_CONF) return;
    if (Reject_Match(cand.angle_deg, cand.distance_cm)) return;

    /* 雷达波束宽、方位粗：落在正被视觉跟踪的目标附近的回波视为同一人，
     * 不更新其方位（视觉更准），也不作为新目标，避免两块板跟同一人。
     * 已确认是人但没有空闲跟踪板的目标仍由雷达更新位置。 */
    uint32_t now = HAL_GetTick();
    TrackedTarget_t *same = NULL;
    float same_diff = 0.0f;
    /* 1.1 m 处约 35°，4 m 以外退回 20° */
    float same_deg = RADAR_SAME_TARGET_BEAM_DEG +
                     atan2f(RADAR_SAME_TARGET_BODY_CM, cand.distance_cm) * (180.0f / 3.14159265f);
    if (same_deg < RADAR_SAME_TARGET_DEG) same_deg = RADAR_SAME_TARGET_DEG;
    for (int i = 0; i < g_targetList.count; i++) {
        TrackedTarget_t *o = &g_targetList.targets[i];
        if (!HasTracker(o->id) ||
            fabsf(BearingFromPoint(o, DEVICE_RADAR_X_CM, DEVICE_RADAR_Y_CM) - cand.angle_deg) >= same_deg) continue;
        /* 视觉不测距，人可能已经走近或走远：距上次更新越久，范围放得越宽 */
        const TargetTrace_t *otr = Trace(o->id);
        float gate = RADAR_SAME_TARGET_CM;
        if (otr && otr->dist_tick) gate += TARGET_WALK_CM_PER_S * (float)(now - otr->dist_tick) / 1000.0f;
        if (gate > RADAR_SAME_TARGET_MAX_CM) gate = RADAR_SAME_TARGET_MAX_CM;
        float diff = fabsf(o->distance_cm - cand.distance_cm);
        if (diff < gate && (!same || diff < same_diff)) {
            same = o;
            same_diff = diff;
        }
    }
#if EXP_RECOG_WHILE_TRACK
    /* 正在识别（含云台转向途中）或排队的候选：雷达再报的还是它，否则同一个人会变成两个目标，
     * 两块板都被占住，就没有板可以接力了。方位以 84E 为准，雷达只更新距离 */
    if (!same) {
        for (int i = 0; i < g_targetList.count; i++) {
            TrackedTarget_t *st = &g_targetList.targets[i];
            if (st->state == TARGET_STATE_TRACKED || st->state == TARGET_STATE_LOST) continue;
            uint8_t active = (st->id == g_active84ETargetId) ? 1U : 0U;
            if (!active && !RecogQueue_Contains(st->id)) continue;
            float seen = (active && g_scan.active && g_scan.has_bearing) ? g_scan.bearing : st->angle_deg;
            if (fabsf(seen - cand.angle_deg) >= same_deg ||
                fabsf(st->distance_cm - cand.distance_cm) >= RADAR_SAME_TARGET_MAX_CM) continue;
            TargetTrace_t *sttr = Trace(st->id);
            st->distance_cm = cand.distance_cm;
            if (sttr) {
                sttr->radar_tick = now;
                sttr->dist_tick = now;
                sttr->radar_dist = cand.distance_cm;
                sttr->radar_angle = cand.angle_deg;
            }
            return;
        }
    }
#endif
    if (same) {
        TargetTrace_t *str = Trace(same->id);
        same->distance_cm = cand.distance_cm;
        if (str) {
            str->dist_tick = now;
            str->radar_tick = now;
            str->radar_dist = cand.distance_cm;
        }
        return;
    }

    int8_t idx = TargetList_FindOrAdd(&g_targetList, &cand);
    if (idx < 0) return;
    TrackedTarget_t *t = &g_targetList.targets[idx];
    TargetTrace_t *tr = Trace(t->id);
    if (tr && tr->discover_tick == 0U) {
        tr->discover_tick = now;
        ExpLog_Event("CAND", t->id, (int32_t)(t->angle_deg * 10.0f),
                     (int32_t)t->distance_cm, 0);
    }
    if (tr) {
        tr->radar_tick = now;
        tr->dist_tick = now;
        tr->radar_dist = cand.distance_cm;
        tr->radar_angle = cand.angle_deg;
    }
    /* 正在跟踪/复锁的目标由视觉负责，雷达只做发现 */
    if (t->state == TARGET_STATE_TRACKED || t->state == TARGET_STATE_LOST) return;

    MotionPredict_Update(t->id, t->angle_deg, t->distance_cm);
    /* 84E空闲则立即预转识别，忙则入队等空闲后自动发送 */
    Request84E(t->id, t->angle_deg, t->distance_cm,
               DEFAULT_84E_WIDTH_DEG, DEFAULT_84E_MARGIN_CM);
}

/* 84E 的最终结论：一帧结果（对照组）或一次边扫描边跟踪的认定 */
static void Recog_Conclude(uint8_t id, float conf, int32_t frames)
{
    uint32_t latency = (g_84ESentTick != 0U) ? (HAL_GetTick() - g_84ESentTick) : 0U;
    g_84EBusy = 0U;
    g_84ESentTick = 0U;
    g_active84ETargetId = 0U;
    DelayRecord_Stop84E(id);

    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);
    ExpLog_Event("RES84E", id, (int32_t)(conf * 1000.0f), (int32_t)latency, frames);

    if (conf < RECOG_HUMAN_CONF) {
        /* 识别为非人体：从未跟踪过的候选直接移除，并短时屏蔽该位置 */
        if (t && t->state != TARGET_STATE_TRACKED && t->state != TARGET_STATE_LOST) {
            TargetTrace_t *tr = Trace(id);
            if (tr && ++tr->reject_count < REJECT_CONFIRM_COUNT) {
                /* 再看一次：排在其他候选之后，不抢占它们 */
                /* 跟踪板已经看到它时方位比雷达准 */
                float again_bearing = HasTracker(id) ? BearingFromPoint(t, DEVICE_RECOG_X_CM, DEVICE_RECOG_Y_CM) : t->angle_deg;
                PendingAim_t again = {1U, id, again_bearing, t->distance_cm,
                                      DEFAULT_84E_WIDTH_DEG, DEFAULT_84E_MARGIN_CM, 0U};
                RecogQueue_Push(&again);
                Dispatch84EFromQueue();
                return;
            }
            /* 并入雷达背景：静止的非人体物体以后不再报；短时屏蔽防止背景未更新前重复上报 */
            {
                TargetTrace_t *atr = Trace(id);
                float radar_angle = atr ? atr->radar_angle : t->angle_deg;
                Radar_Process_Absorb(radar_angle);
                Reject_Add(radar_angle, t->distance_cm);
            }
            ReleaseTarget(id, 0);
        }
        if (!Dispatch84EFromQueue()) {
            g_currentProc = PROC_IDLE;
        }
        return;
    }

    {
        TargetTrace_t *rtr = Trace(id);
        if (rtr) rtr->reject_count = 0U;
    }
    /* 复锁中：84E 只说明那里有人，要等跟踪板自己重新看到才算复锁成功 */
    if (t && t->state == TARGET_STATE_LOST && HasTracker(id)) {
        DataFusion_UpdateWith84E(&g_fusion, id, conf, GimbalBearing(GIMBAL_CAM2),
                                 t->distance_cm, HAL_GetTick());
#if EXP_RECOG_WHILE_TRACK
        /* 84E 看到它了：把新坐标告诉主板 */
        if (g_scanHandoff && g_scanHandoff->has_bearing && t->esp_assigned <= 1U) {
            float aim = ScanAimBearing(g_scanHandoff, t, t->esp_assigned);
            AimESP(t->esp_assigned, t->id, aim, aim, t->distance_cm);
        }
#endif
        Dispatch84EFromQueue();
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

#if EXP_RECOG_WHILE_TRACK
/* 84E 边扫描边跟踪的一帧：x/y 是目标在画面中的像素位置，看不到时为负 */
static void Scan_OnFrame(uint8_t id, float conf, float x, float y)
{
    uint32_t now = HAL_GetTick();
    TrackedTarget_t *t = TargetList_GetTarget(&g_targetList, id);
    if (!t) {
        Cmd_84E_SendStopCmd(id);
        g_scan.active = 0U;
        Recog_Conclude(id, 0.0f, g_scan.frames);
        return;
    }
    if (g_scan.frames < 255U) g_scan.frames++;
    g_scan.conf_sum += conf;

    /* 跟着目标转云台 */
    if (x >= 0.0f) {
        float off = (x - E84_IMG_WIDTH_PX / 2.0f) * (E84_CAM_FOV_H_DEG / E84_IMG_WIDTH_PX);
        float bearing = GimbalBearing(GIMBAL_CAM2) + off;
        if (g_scan.has_bearing && now != g_scan.bearing_tick) {
            float vel = (bearing - g_scan.bearing) * 1000.0f / (float)(now - g_scan.bearing_tick);
            if (vel > SCAN_MAX_VEL_DEG_S) vel = SCAN_MAX_VEL_DEG_S;
            if (vel < -SCAN_MAX_VEL_DEG_S) vel = -SCAN_MAX_VEL_DEG_S;
            g_scan.vel_deg_s += 0.3f * (vel - g_scan.vel_deg_s);
        }
        g_scan.bearing = bearing;
        g_scan.bearing_tick = now;
        g_scan.has_bearing = 1U;
        float step = off * SCAN_PAN_GAIN;
        if (step > SCAN_PAN_STEP_MAX) step = SCAN_PAN_STEP_MAX;
        if (step < -SCAN_PAN_STEP_MAX) step = -SCAN_PAN_STEP_MAX;
        Gimbal_SetPan(GIMBAL_CAM2, Gimbal_GetEstimatedPan(GIMBAL_CAM2) + step);
        if (y >= 0.0f) {
            float voff = (y - E84_IMG_HEIGHT_PX / 2.0f) * (E84_CAM_FOV_V_DEG / E84_IMG_HEIGHT_PX);
            Gimbal_ApplyTrackingControl(GIMBAL_CAM2, 0.0f, voff * SCAN_TILT_GAIN);
        }
        /* 跟踪板还没上时，目标的方位就是 84E 看到的 */
        if (t->state != TARGET_STATE_TRACKED && t->state != TARGET_STATE_LOST) t->angle_deg = bearing;
    }

    uint8_t human = (conf >= RECOG_HUMAN_CONF) ? 1U : 0U;
    if (human) {
        g_scan.consec_human++;
    } else {
        g_scan.consec_human = 0U;
        if (g_scan.low_frames < 255U) g_scan.low_frames++;
    }

    uint8_t confirmed = (g_scan.consec_human >= SCAN_CONFIRM_FRAMES) ? 1U : 0U;
    if (!confirmed && g_scan.low_frames < SCAN_REJECT_FRAMES &&
        (now - g_scan.start_tick) < SCAN_MAX_MS) {
        g_84ESentTick = now;    /* 还在回传就没有超时 */
        return;
    }

    /* 有结论了：84E 停下待命 */
    ScanTrack_t done = g_scan;
    float avg = done.conf_sum / (float)done.frames;
    Cmd_84E_SendStopCmd(id);
    g_scan.active = 0U;
    g_84ESentTick = done.start_tick ? done.start_tick : 1U;
    /* 雷达杂散回波把 84E 引到已被跟踪的人身上时，看到的“人”不是新目标，不能再占一块跟踪板 */
    if (confirmed && done.has_bearing) {
        for (int i = 0; i < g_targetList.count; i++) {
            TrackedTarget_t *o = &g_targetList.targets[i];
            if (o->id == id || !HasTracker(o->id)) continue;
            if (fabsf(BearingFromPoint(o, DEVICE_RECOG_X_CM, DEVICE_RECOG_Y_CM) - done.bearing) < SCAN_SAME_TARGET_DEG) {
                confirmed = 0U;
                break;
            }
        }
    }
    if (!confirmed) {
        /* 这么多帧都不像人，已经比单帧判定可靠，不用再看第二次 */
        TargetTrace_t *tr = Trace(id);
        if (tr) tr->reject_count = REJECT_CONFIRM_COUNT;
        Recog_Conclude(id, (avg < RECOG_HUMAN_CONF) ? avg : 0.0f, done.frames);
        return;
    }
    g_scanHandoff = &done;
    Recog_Conclude(id, (avg >= RECOG_HUMAN_CONF) ? avg : RECOG_HUMAN_CONF, done.frames);
    g_scanHandoff = NULL;
}
#endif

static void On84EResult(uint8_t id, float conf, float x, float y, float dist, float angle)
{
    (void)dist; (void)angle;                     /* 84E 不测距，距离以 H7 侧为准 */
    if (g_shutdownRequested) return;             /* 停止期间不再分配跟踪 */
#if EXP_RECOG_WHILE_TRACK
    /* 停止命令发出后，路上可能还有几帧 */
    if (g_scan.active && id == g_active84ETargetId) Scan_OnFrame(id, conf, x, y);
#else
    (void)x; (void)y;
    Recog_Conclude(id, conf, 0);
#endif
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
#if EXP_RECOG_WHILE_TRACK
        /* 刚交给这块板，它还在找目标：一帧没看到就算跟丢，会拿旧坐标去复锁，反而把人放跑 */
        if (t->state == TARGET_STATE_TRACKED && tr && !tr->lock_logged &&
            (now - g_espLastResend[esp_id]) < TRACK_ACQUIRE_MS) {
            return;
        }
#endif
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
        /* 辅助板没看到目标（人动了，或距离估得不准）：按主板最新的方位再瞄一次 */
        if (t->state == TARGET_STATE_TRACKED && !g_pendingESP[esp_id].active &&
            (now - g_espLastResend[esp_id]) >= ASSIST_REAIM_MS) {
            float aim = AssistLocalize_ComputeBearing(t);
            if (EspCovers(esp_id, aim)) AimESP(esp_id, t->id, aim, aim, t->distance_cm);
        }
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
                 (int32_t)(tri_dist - tr->radar_dist));
    if (tri_dist >= RADAR_NEAR_LIMIT_CM && tri_dist <= RADAR_FAR_CM) {
        t->distance_cm += TRI_DIST_BLEND * (tri_dist - t->distance_cm);
        tr->dist_tick = now;
    }
}

static void OnESP32Result(uint8_t esp_id, uint8_t id,
                          float x, float y,
                          float pan_ctrl, float tilt_ctrl,
                          uint8_t lost)
{
    if (esp_id > 1 || g_shutdownRequested) return;
    uint32_t now = HAL_GetTick();
    g_lastEspRx[esp_id] = now;
    g_espLastFrame[esp_id] = now;
    g_espResends[esp_id] = 0U;
    g_espSilent[esp_id] = 0U;
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
            /* 方位的基准换成了新板，旧历史会算出离谱的速度（两块板看同一个人能差 360°） */
            MotionPredict_Reset(id);
            RelockLogic_ResetHistory(id, DataFusion_ESP32Bearing(GimbalBearing((GimbalId_t)esp_id), x), t->distance_cm);
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
    if (g_espSilent[to_esp]) return;
    float bearing = BearingFromBoard(t, to_esp);
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

    /* 84E 看得到才一起确认，否则白转一趟，还会被当成一次“不是人” */
    float recog_bearing = BearingFromPoint(t, DEVICE_RECOG_X_CM, DEVICE_RECOG_Y_CM);
    if (!g_84EBusy && Cam84ECovers(recog_bearing)) {
        Request84E(t->id, recog_bearing, t->distance_cm,
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

/* 主板重发后仍无回传：改派另一块板，把原板标为无回传。
 * 另一块板在忙、也无回传过或转不到该方位时返回 0，由调用方继续重发。
 * 这里按硬限位判断，不用接力的 EspCovers：原板已经不工作，靠近边界也比没人跟好。 */
static uint8_t FailoverPrimary(uint8_t from_esp, TrackedTarget_t *t, uint32_t silent_ms)
{
    if (g_handover.active) return 0U;
    uint8_t to_esp = (uint8_t)(1U - from_esp);
    if (g_espRole[to_esp] == ESP_ROLE_TRACK || g_espRole[to_esp] == ESP_ROLE_HANDOVER) return 0U;
    /* 主板沉默这几秒方位没有更新，人可能已经走开：先外推到现在 */
    TrackedTarget_t now_t = *t;
#if EXP_PREDICTIVE_AIM
    /* 只有真跟踪过才有可信的速度；主板从未回传时 silent_ms 是从开机算起的，不能外推 */
    const TargetTrace_t *ptr = Trace(t->id);
    float pb, pd, spread;
    if (ptr && ptr->lock_logged && silent_ms < 10000U &&
        MotionPredict_Get(t->id, (float)silent_ms / 1000.0f, &pb, &pd, &spread)) {
        now_t.angle_deg = pb;
        now_t.distance_cm = pd;
    }
#endif
    float bearing = BearingFromBoard(&now_t, to_esp);
    float pan = BearingToPanRaw((GimbalId_t)to_esp, bearing);
    if (g_espSilent[to_esp] || pan < GIMBAL_PAN_HARD_MIN || pan > GIMBAL_PAN_HARD_MAX) return 0U;

    ExpLog_Event("FAILOVER", t->id, from_esp, to_esp, (int32_t)silent_ms);
    Logger_Print(LOG_WARN, "ESP32-%c no reply, target %u moved to ESP32-%c",
                 (from_esp == 0U) ? 'A' : 'B', t->id, (to_esp == 0U) ? 'A' : 'B');
    EspRelease(from_esp);
    g_espSilent[from_esp] = 1U;
    EspAssign(to_esp, ESP_ROLE_TRACK, t->id);
    t->esp_assigned = to_esp;
    t->angle_deg = bearing;
    MotionPredict_Reset(t->id);
    RelockLogic_ResetHistory(t->id, bearing, t->distance_cm);
    AimESP(to_esp, t->id, bearing, bearing, t->distance_cm);
    return 1U;
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
     * 首选板无回传过时也优先另一块；另一块在忙时仍派给它，它一回传就恢复可用。
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
    float aim_pref = t->angle_deg, aim_other = t->angle_deg;
#if EXP_RECOG_WHILE_TRACK
    if (g_scanHandoff && g_scanHandoff->has_bearing) {
        aim_pref = ScanAimBearing(g_scanHandoff, t, esp_id);
        aim_other = ScanAimBearing(g_scanHandoff, t, other);
    }
#endif
    if (busy_pref || ((!EspCovers(esp_id, aim_pref) || g_espSilent[esp_id]) &&
                      !busy_other && EspCovers(other, aim_other))) {
        esp_id = other;
        aim_pref = aim_other;
    }
    TargetTrace_t *tr = Trace(t->id);
    if (g_espRole[esp_id] == ESP_ROLE_TRACK ||
        g_espRole[esp_id] == ESP_ROLE_HANDOVER) {
        /* 两块跟踪板都在忙：按开题方案把该目标交给局域网客户端 + 语音播报（只报一次）；
         * 目标保留在列表中，由雷达更新位置，有板空出时自动补上（Process_CheckClosedLoop） */
        t->esp_assigned = TARGET_ESP_NONE;
        if (tr && !tr->third_reported) {
            tr->third_reported = 1U;
            ExpLog_Event("THIRD", t->id, (int32_t)(t->angle_deg * 10.0f),
                         (int32_t)t->distance_cm, 0);
            Logger_Print(LOG_INFO, "Target %u: no free ESP32, reported to LAN client", t->id);
            TargetSwitch_Execute(&g_targetList, t->id);
        }
        return;
    }

    EspAssign(esp_id, ESP_ROLE_TRACK, t->id);
    t->esp_assigned = esp_id;
    if (tr) {
        tr->lock_logged = 0U;
        tr->third_reported = 0U;
    }
    /* 目标方位的基准从这里起是这块板（84E 给的方位基准不同） */
    t->angle_deg = aim_pref;
    AimESP(esp_id, t->id, aim_pref, aim_pref, t->distance_cm);
}

static void Process_CheckClosedLoop(void)
{
    static uint32_t last_check_tick = 0;
    static uint32_t last_fps_tick = 0;
    static uint32_t last_comm_tick = 0;
    static uint32_t last_gim_tick = 0;
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

    /* 云台朝向，供监控界面显示 */
    if ((now - last_gim_tick) >= 200U) {
        last_gim_tick = now;
        ExpLog_Event("GIM", 0,
                     (int32_t)(Gimbal_GetEstimatedPan(GIMBAL_CAM0) * 10.0f),
                     (int32_t)(Gimbal_GetEstimatedPan(GIMBAL_CAM1) * 10.0f),
                     (int32_t)(Gimbal_GetEstimatedPan(GIMBAL_CAM2) * 10.0f));
    }

    /* 板间通信统计（实验指标：丢帧/校验错误率），输出累计值 */
    if ((now - last_comm_tick) >= COMM_REPORT_MS) {
        last_comm_tick = now;
        for (uint8_t ch = 0; ch < PROTO_CH_COUNT; ch++) {
            Protocol_CommStats_t s;
            Protocol_GetCommStats(ch, &s);
            ExpLog_Event("COMM", ch, (int32_t)s.frames,
                         (int32_t)(s.xor_errors + s.bad_len),
                         (int32_t)(s.queue_drops + s.uart_errors));
        }
    }

    /* 辅助板的目标已不存在 → 释放 */
    for (uint8_t i = 0; i < 2; i++) {
        if (g_espRole[i] == ESP_ROLE_ASSIST &&
            !TargetList_GetTarget(&g_targetList, g_espTarget[i])) {
            EspRelease(i);
        }
    }

    /* 没有跟踪板、雷达也很久没再报出的目标（人已离开、84E 超时、确认是人但一直
     * 等不到空闲板）→ 移出列表，否则 3 个槽位会被占满。正在送 84E 识别或排队中的不清理。 */
    for (int i = g_targetList.count - 1; i >= 0; i--) {
        TrackedTarget_t *t = &g_targetList.targets[i];
        TargetTrace_t *tr = Trace(t->id);
        if (!HasTracker(t->id) && t->state != TARGET_STATE_LOST &&
            tr && tr->radar_tick != 0U && (now - tr->radar_tick) > STALE_CANDIDATE_MS &&
            t->id != g_active84ETargetId && !RecogQueue_Contains(t->id)) {
            ReleaseTarget(t->id, 2);
        }
    }

    /* 已确认是人但没有跟踪板的目标：有板空闲（或只在做辅助）时补上 */
    if (!g_handover.active) {
        for (int i = 0; i < g_targetList.count; i++) {
            TrackedTarget_t *t = &g_targetList.targets[i];
            if (t->state != TARGET_STATE_TRACKED || HasTracker(t->id)) continue;
            uint8_t free_board = 0U;
            for (uint8_t e = 0; e < 2; e++) {
                if ((g_espRole[e] == ESP_ROLE_IDLE || g_espRole[e] == ESP_ROLE_ASSIST) &&
                    !g_pendingESP[e].active) free_board = 1U;
            }
            if (free_board) Process_InvokeESP32(t);
        }
    }

    for (int i = 0; i < g_targetList.count; i++) {
        TrackedTarget_t *t = &g_targetList.targets[i];

        TargetTrace_t *tr = Trace(t->id);
        const FusedTarget_t *ft = DataFusion_Get(&g_fusion, t->id);
        if (tr && ft && (now - tr->fuse_log_tick) >= 200U) {
            tr->fuse_log_tick = now;
            ExpLog_Event("FUSE", t->id, (int32_t)ft->x, (int32_t)ft->y, ft->updated_by);
        }

        if ((t->state != TARGET_STATE_TRACKED && t->state != TARGET_STATE_LOST) ||
            t->esp_assigned > 1U) continue;
        uint8_t p = t->esp_assigned;
        if (g_espRole[p] != ESP_ROLE_TRACK || g_espTarget[p] != t->id) continue;

        /* 链路看门狗：主板长时间无回传时限频重发，重发后仍无回传就改派另一块板 */
        if (!g_pendingESP[p].active &&
            ClosedLoop_NeedsResend(t, now - g_espLastFrame[p],
                                   now - g_espLastResend[p])) {
            if (g_espResends[p] >= CLOSED_LOOP_FAILOVER_RESENDS &&
                FailoverPrimary(p, t, now - g_espLastFrame[p])) continue;
            if (g_espResends[p] < CLOSED_LOOP_FAILOVER_RESENDS) g_espResends[p]++;
            AimESP(p, t->id, t->angle_deg, t->angle_deg, t->distance_cm);
        }

#if EXP_ASSIST_ENABLE
        /* 单目标：另一块空闲且能覆盖该方位的板启动一次辅助观察（不重复下发）。
         * 无回传过的板不用：云台白转，它又可能被当成空闲板占住位置。 */
        uint8_t other = (uint8_t)(1U - p);
        float assist_bearing = AssistLocalize_ComputeBearing(t);
        if (t->state == TARGET_STATE_TRACKED &&
            g_targetList.count == 1 && !g_handover.active &&
            g_espRole[other] == ESP_ROLE_IDLE && !g_pendingESP[other].active &&
            !g_espSilent[other] && EspCovers(other, assist_bearing)) {
            EspAssign(other, ESP_ROLE_ASSIST, t->id);
            AimESP(other, t->id, assist_bearing, assist_bearing, t->distance_cm);
        }
#endif
    }
}
