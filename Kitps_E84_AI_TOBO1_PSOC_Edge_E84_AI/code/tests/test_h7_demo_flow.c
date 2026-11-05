/*
 * 整机流程仿真：按答辩演示的顺序跑一遍，雷达模块（距离图、背景学习、变化检测）和调度器都是真实代码，
 * 只把超声波测距、84E、ESP32 换成根据“虚拟房间”计算结果的替身。
 *
 * 同时把全部日志写到 build/demo_flow.log，可用 tools/demo_monitor.html 回放来排练。
 * 这些都是仿真结果，不代表实物性能。
 */
#include "test.h"
#include "h7_stubs.h"
#include "main.h"
#include "scheduler.h"
#include "gimbal.h"
#include "servo.h"
#include "radar_driver.h"
#include "radar_process.h"
#include "config_mount.h"
#include <stdlib.h>
#include <string.h>

TEST_MAIN_DEFS

#define DEG (3.14159265f / 180.0f)
#define BEAM_DEG 8.0f

/* ---------- 虚拟房间 ---------- */
typedef struct { int on; float x, y, vx, vy; int human; int hidden; } Obj_t;
enum { PERSON_A, PERSON_B, CHAIR, OBJ_COUNT };
static Obj_t g_obj[OBJ_COUNT];

static float bearing_from(float ox, float oy, float x, float y)
{
    return atan2f(x - ox, y - oy) / DEG;
}

/* 雷达射线打到房间四壁的距离：x ±320 cm，y -150~360 cm */
static float wall_range(float bearing)
{
    float dx = sinf(bearing * DEG), dy = cosf(bearing * DEG);
    float best = 1e9f;
    if (dx > 1e-3f)  { float t = (320.0f - DEVICE_RADAR_X_CM) / dx;  if (t < best) best = t; }
    if (dx < -1e-3f) { float t = (-320.0f - DEVICE_RADAR_X_CM) / dx; if (t < best) best = t; }
    float ry = DEVICE_RADAR_Y * 100.0f;
    if (dy > 1e-3f)  { float t = (360.0f - ry) / dy;  if (t < best) best = t; }
    if (dy < -1e-3f) { float t = (-150.0f - ry) / dy; if (t < best) best = t; }
    return best;
}

static float world_range(float bearing)
{
    float r = wall_range(bearing);
    float ry = DEVICE_RADAR_Y * 100.0f;
    for (int i = 0; i < OBJ_COUNT; i++) {
        if (!g_obj[i].on || g_obj[i].hidden) continue;
        float b = bearing_from(DEVICE_RADAR_X_CM, ry, g_obj[i].x, g_obj[i].y);
        float d = hypotf(g_obj[i].x - DEVICE_RADAR_X_CM, g_obj[i].y - ry);
        if (fabsf(b - bearing) <= BEAM_DEG && d < r) r = d;
    }
    return r + (float)(rand() % 5 - 2);
}

/* ---------- 超声波替身 ---------- */
static int s_ping_busy;
static uint32_t s_ping_done;
static float s_ping_cm;

void Radar_Driver_Init(void) { s_ping_busy = 0; }
RadarStatus_t Radar_Trigger(void)
{
    if (s_ping_busy) return RADAR_BUSY;
    s_ping_cm = world_range(Servo_GetAngle() - SERVO_ANGLE_CENTER);
    s_ping_done = stub_tick + (uint32_t)(s_ping_cm * 58.0f / 1000.0f) + 1U;
    s_ping_busy = 1;
    return RADAR_OK;
}
RadarResult_t Radar_Poll(float *dist_cm)
{
    if (!s_ping_busy || stub_tick < s_ping_done) return RADAR_RESULT_NONE;
    s_ping_busy = 0;
    if (s_ping_cm > RADAR_MAX_DISTANCE_CM) return RADAR_RESULT_NO_ECHO;
    *dist_cm = s_ping_cm;
    return RADAR_RESULT_OK;
}

/* ---------- 84E 替身：边扫描边跟踪，每 60 ms 回传一帧画面里有没有人、人在哪 ---------- */
static int s_84e_done, s_84e_stop_done, s_84e_on;
static uint32_t s_84e_due;
static int s_id_obj[256];
static float gimbal_bearing(int g);

static void sim_84e(void)
{
    /* 命令和停止按先后配对：停止数追上开始数就不再回传 */
    if (s_84e_stop_done < stub_84e_stop_n) {
        s_84e_stop_done = stub_84e_stop_n;
        s_84e_done = stub_84e_n;
        s_84e_on = 0;
    }
    if (!s_84e_on && s_84e_done < stub_84e_n) { s_84e_on = 1; s_84e_due = stub_tick + 60U; }
    if (!s_84e_on || stub_tick < s_84e_due) return;
    s_84e_due = stub_tick + 60U;
    const Rec84E_t *c = &stub_84e[stub_84e_n - 1];
    float conf = 0.05f, x = -1.0f, y = -1.0f;
    float best = 25.0f, heading = gimbal_bearing(2);
    for (int i = 0; i < OBJ_COUNT; i++) {
        if (!g_obj[i].on || g_obj[i].hidden) continue;
        float e = bearing_from(0, 0, g_obj[i].x, g_obj[i].y) - heading;
        if (fabsf(e) < best) {
            best = fabsf(e);
            conf = g_obj[i].human ? 0.90f : 0.10f;
            x = 80.0f + e * (160.0f / 50.0f);
            y = 60.0f;
            if (g_obj[i].human) s_id_obj[c->id] = i;
        }
    }
    stub_cb84e(c->id, conf, x, y, (c->dmin + c->dmax) / 2.0f, c->angle);
}

/* ---------- ESP32 替身：目标在视野内就回传图像位置和云台控制量 ---------- */
/* 和 ESP32_ov2640/tracking/closed_loop.c 相同（tools/sim_wasm/test_live_sim.js 会核对） */
#define ESP_KP       2.5f
#define ESP_KI       0.05f
#define ESP_KD       0.5f
#define ESP_I_LIMIT  200.0f
#define ESP_CTRL_MAX 400.0f
static int s_trk_done, s_rel_done;
static uint8_t s_board_id[2];
static int s_miss[2];
static float s_pid_i[2], s_pid_prev[2];

static float esp_pid(int e, float err_mm)
{
    s_pid_i[e] += err_mm;
    if (s_pid_i[e] > ESP_I_LIMIT) s_pid_i[e] = ESP_I_LIMIT;
    if (s_pid_i[e] < -ESP_I_LIMIT) s_pid_i[e] = -ESP_I_LIMIT;
    float out = ESP_KP * err_mm + ESP_KI * s_pid_i[e] + ESP_KD * (err_mm - s_pid_prev[e]);
    s_pid_prev[e] = err_mm;
    if (out > ESP_CTRL_MAX) out = ESP_CTRL_MAX;
    if (out < -ESP_CTRL_MAX) out = -ESP_CTRL_MAX;
    return out;
}

static float gimbal_bearing(int g)
{
    static const float off[3] = { MOUNT_CAM0_PAN_OFFSET, MOUNT_CAM1_PAN_OFFSET, MOUNT_CAM2_PAN_OFFSET };
    return Gimbal_GetEstimatedPan((GimbalId_t)g) - 135.0f + off[g];
}

static void sim_esp(void)
{
    for (; s_rel_done < stub_release_n; s_rel_done++) s_board_id[stub_release[s_rel_done].esp] = 0;
    for (; s_trk_done < stub_track_n; s_trk_done++) {
        s_board_id[stub_track[s_trk_done].esp] = stub_track[s_trk_done].id;
        s_miss[stub_track[s_trk_done].esp] = 0;
        s_pid_i[stub_track[s_trk_done].esp] = 0.0f;   /* Tracker_StartAt 会重置闭环 */
        s_pid_prev[stub_track[s_trk_done].esp] = 0.0f;
    }
    for (int e = 0; e < 2; e++) {
        uint8_t id = s_board_id[e];
        if (id == 0) continue;
        const Obj_t *o = &g_obj[s_id_obj[id]];
        float ox = (e == 0) ? DEVICE_TRACK_A_X_CM : DEVICE_TRACK_B_X_CM;
        float err = bearing_from(ox, 0, o->x, o->y) - gimbal_bearing(e);
        if (o->on && !o->hidden && fabsf(err) < 25.0f) {
            s_miss[e] = 0;
            float px = 80.0f + err * (160.0f / 50.0f);
            float x_mm = px * 1.875f;
            stub_cbesp((uint8_t)e, id, x_mm, 112.5f, esp_pid(e, x_mm - 150.0f), 0.0f, 0);
        } else if (++s_miss[e] >= 5) {
            s_pid_i[e] = 0.0f;
            s_pid_prev[e] = 0.0f;
            stub_cbesp((uint8_t)e, id, 0, 0, 0, 0, 1);
        }
    }
}

/* ---------- 时间推进 ---------- */
static void run_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        stub_tick++;
        for (int k = 0; k < OBJ_COUNT; k++) {
            g_obj[k].x += g_obj[k].vx / 1000.0f;
            g_obj[k].y += g_obj[k].vy / 1000.0f;
        }
        sim_84e();
        if (stub_tick % 33U == 0U) sim_esp();
        Scheduler_Run();
    }
}

static int wait_event(const char *name, int count, uint32_t timeout_ms)
{
    for (uint32_t t = 0; t < timeout_ms; t += 10) {
        if (stub_event_count(name) >= count) return 1;
        run_ms(10);
    }
    return 0;
}

static int wait_background(uint32_t timeout_ms)
{
    for (uint32_t t = 0; t < timeout_ms; t += 100) {
        if (Radar_Process_BackgroundReady()) return 1;
        run_ms(100);
    }
    return 0;
}

static void phase(const char *name)
{
    printf("  [%6.1f s] %s\n", (double)stub_tick / 1000.0, name);
}

int main(void)
{
    printf("[demo flow]\n");
    srand(1);
    stub_reset();
    stub_log_fp = fopen("build/demo_flow.log", "w");
    if (stub_log_fp) fprintf(stub_log_fp, "# SOURCE=SIMULATION tests/test_h7_demo_flow.c\n");

    Servo_Init();
    Gimbal_Init();
    Radar_Process_Init();
    Scheduler_Init();

    phase("1 empty room: learn background");
    CHECK(wait_background(90000), "background should be learned within 90 s");
    CHECK(stub_event_count("BGOK") == 1, "BGOK event");
    run_ms(13000);
    CHECK(stub_event_count("CAND") == 0, "an empty room must produce no candidates, got %d", stub_event_count("CAND"));

    phase("2 person A walks in");
    g_obj[PERSON_A] = (Obj_t){ 1, -100.0f, 200.0f, 0, 0, 1, 0 };
    CHECK(wait_event("CAND", 1, 15000), "radar should find person A");
    CHECK(wait_event("LOCK", 1, 8000), "ESP32 should lock person A");
    run_ms(2000);
    CHECK(stub_event_count("RELEASE") == 0, "person A stays tracked");

    phase("3 person A crosses the gimbal limit: hand over");
    g_obj[PERSON_A].vx = 40.0f;
    CHECK(wait_event("HOSTART", 1, 15000), "handover should start");
    CHECK(wait_event("HOOK", 1, 6000), "handover should finish");
    g_obj[PERSON_A].vx = 0.0f;
    run_ms(2000);

    phase("3b person A walks away from the radar: same target");
    int cand_walk = stub_event_count("CAND");
    g_obj[PERSON_A].vy = 25.0f;
    run_ms(3200);
    g_obj[PERSON_A].vy = 0.0f;
    run_ms(14000);
    CHECK(stub_event_count("CAND") == cand_walk, "walking away must not create a new target, got %d more",
          stub_event_count("CAND") - cand_walk);
    CHECK(stub_event_count("RELEASE") == 0, "person A still tracked");

    phase("4 person B walks in");
    int cand_before = stub_event_count("CAND");
    g_obj[PERSON_B] = (Obj_t){ 1, -120.0f, 180.0f, 0, 0, 1, 0 };
    CHECK(wait_event("CAND", cand_before + 1, 15000), "radar should find person B");
    CHECK(wait_event("LOCK", 2, 8000), "second board should lock person B");
    run_ms(3000);

    phase("5 person B is hidden for a moment: relock");
    g_obj[PERSON_B].hidden = 1;
    CHECK(wait_event("LOST", 1, 3000), "LOST event");
    run_ms(1200);
    g_obj[PERSON_B].hidden = 0;
    CHECK(wait_event("RELOCK", 1, 6000), "RELOCK event after reappearing");
    run_ms(2000);

    phase("6 a chair is placed: rejected, then ignored");
    int rel_before = stub_event_count("RELEASE");
    int cand_chair = stub_event_count("CAND");
    g_obj[CHAIR] = (Obj_t){ 1, 150.0f, 60.0f, 0, 0, 0, 0 };   /* 放在雷达扫描范围内 */
    CHECK(wait_event("CAND", cand_chair + 1, 15000), "radar should notice the chair");
    CHECK(wait_event("RELEASE", rel_before + 1, 5000), "84E rejects the chair, candidate released");
    int cand_after = stub_event_count("CAND");
    run_ms(26000);
    CHECK(stub_event_count("CAND") == cand_after, "chair absorbed into background: no new candidates, got %d more",
          stub_event_count("CAND") - cand_after);
    CHECK(stub_event_count("LOCK") == 2, "chair must not get an ESP32");

    phase("7 STOP, then START");
    Scheduler_RequestShutdown();
    run_ms(3000);
    CHECK(Scheduler_IsShutdownComplete(), "homing completes");
    for (int g = 0; g < 3; g++) CHECK_NEAR(Gimbal_GetEstimatedPan((GimbalId_t)g), 135.0, 0.5, "gimbal homed");
    int lock_before = stub_event_count("LOCK");
    Scheduler_Resume();
    CHECK(wait_event("LOCK", lock_before + 1, 20000), "people are found again after START");

    phase("8 everyone leaves, RELEARN");
    g_obj[PERSON_A].on = g_obj[PERSON_B].on = g_obj[CHAIR].on = 0;
    Scheduler_RelearnBackground();
    CHECK(stub_event_count("RELEARN") == 1, "RELEARN event");
    CHECK(!Radar_Process_BackgroundReady(), "background cleared");
    CHECK(wait_background(90000), "background relearned");
    CHECK(stub_event_count("BGOK") == 2, "second BGOK");
    int cand_end = stub_event_count("CAND");
    run_ms(13000);
    CHECK(stub_event_count("CAND") == cand_end, "no candidates in the empty room after relearning");
    phase("done");

    if (stub_log_fp) fclose(stub_log_fp);
    stub_log_fp = NULL;
    TEST_REPORT("demo_flow");
}
