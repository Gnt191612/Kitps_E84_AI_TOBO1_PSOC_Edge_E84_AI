/*
 * H7 调度器场景测试：真实 scheduler.c + 云台/舵机/目标列表/复锁/融合/预测/实验记录，
 * 只把 HAL、雷达测量、串口收发换成替身（stubs/h7_stubs.c）。
 *
 * 用法：test_h7_scheduler        依次在子进程里跑全部场景
 *       test_h7_scheduler <n>    只跑第 n 个场景（调度器内部状态不可重置，故每个场景单独进程）
 */
#include "test.h"
#include "h7_stubs.h"
#include "scheduler.h"
#include "gimbal.h"
#include "config_mount.h"
#include <stdlib.h>
#include <string.h>

TEST_MAIN_DEFS

/* ---------- 仿真工具 ---------- */
static void run_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        stub_tick++;
        Scheduler_Run();
    }
}

/* 方位 b（相对正前方）在某块板图像中的 x（伪 mm），该板云台当前指向方位 gb */
static float esp_x_for(float b, float gb)
{
    float px = 80.0f + (b - gb) * (160.0f / 50.0f);
    return px * 1.875f;
}

static float gimbal_bearing(int g)
{
    static const float off[3] = { MOUNT_CAM0_PAN_OFFSET, MOUNT_CAM1_PAN_OFFSET, MOUNT_CAM2_PAN_OFFSET };
    return Gimbal_GetEstimatedPan((GimbalId_t)g) - 135.0f + off[g];
}

/* 以 30 帧/秒持续回传 ms 毫秒；true_b 为目标真实方位（相对该板位置） */
static void feed(int esp, uint8_t id, float true_b, uint8_t lost, float pan_ctrl, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 33) {
        float x = esp_x_for(true_b, gimbal_bearing(esp));
        stub_cbesp((uint8_t)esp, id, x, 112.5f, pan_ctrl, 0.0f, lost);
        run_ms(33);
    }
}

/* 运行直到 84E 命令数超过 n，返回新命令下标；超时返回 -1 */
static int wait_84e(int n, uint32_t timeout_ms)
{
    for (uint32_t t = 0; t < timeout_ms; t++) {
        if (stub_84e_n > n) return n;
        run_ms(1);
    }
    return -1;
}

static int wait_track(uint8_t esp, int n_before, uint32_t timeout_ms)
{
    for (uint32_t t = 0; t < timeout_ms; t++) {
        if (stub_track_count(esp) > n_before) return 1;
        run_ms(1);
    }
    return 0;
}

static void boot(void)
{
    stub_reset();
    Gimbal_Init();
    Scheduler_Init();
    run_ms(10);
}

/* 发现一个目标、84E 确认为人、等主板收到跟踪命令；返回目标 id */
static uint8_t acquire(float bearing, float dist, int expect_esp)
{
    int n = stub_84e_n;
    stub_push_candidate(bearing, dist, 0.9f);
    int k = wait_84e(n, 3000);
    CHECK(k >= 0, "radar candidate should trigger an 84E command");
    if (k < 0) return 0;
    uint8_t id = stub_84e[k].id;
    CHECK_NEAR(stub_84e[k].angle, bearing, 2.0, "84E aimed bearing");
    int before = stub_track_count((uint8_t)expect_esp);
    stub_cb84e(id, 0.9f, 0, 0, dist, bearing);
    CHECK(wait_track((uint8_t)expect_esp, before, 4000), "ESP32-%c should get a TRACK command", 'A' + expect_esp);
    return id;
}

/* ---------- 场景 ---------- */

/* 1. 单目标：跟踪命令只发一次；辅助板只启动一次；三角定位正确 */
static void s1_single_target(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    const RecTrack_t *tr = &stub_track[stub_track_n - 1];
    CHECK_NEAR(tr->angle, 0.0, 1.0, "TRACK param1 must be offset from gimbal heading (~0)");

    /* 真实目标在平面 (0,200)：相对 A(-20,0) 方位 +5.71°，相对 B(20,0) 方位 -5.71° */
    feed(0, id, 5.71f, 0, 0.0f, 3000);
    CHECK(stub_track_count(0) == 1, "primary must receive TRACK exactly once, got %d", stub_track_count(0));
    CHECK(stub_event_count("LOCK") == 1, "LOCK event expected");
    CHECK(stub_track_count(1) == 1, "assist board started exactly once, got %d", stub_track_count(1));

    /* 两板同时回传 → 三角定位 */
    for (int i = 0; i < 30; i++) {
        stub_cbesp(0, id, esp_x_for(5.71f, gimbal_bearing(0)), 112.5f, 0, 0, 0);
        stub_cbesp(1, id, esp_x_for(-5.71f, gimbal_bearing(1)), 112.5f, 0, 0, 0);
        run_ms(33);
    }
    long x = 0, y = 0, d = 0;
    CHECK(stub_event_last("TRI", &x, &y, &d), "TRI event expected");
    CHECK_NEAR(x, 0.0, 3.0, "triangulated x (cm)");
    CHECK_NEAR(y, 200.0, 6.0, "triangulated y (cm)");
}

/* 2. 辅助板报丢失不得影响主板目标 */
static void s2_assist_lost_harmless(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    feed(0, id, 5.71f, 0, 0.0f, 1000);
    int n84 = stub_84e_n;
    for (int i = 0; i < 90; i++) {           /* 3 s：主板正常、辅助板丢失 */
        stub_cbesp(0, id, esp_x_for(5.71f, gimbal_bearing(0)), 112.5f, 0, 0, 0);
        stub_cbesp(1, id, 150.0f, 112.5f, 0, 0, 1);
        run_ms(33);
    }
    CHECK(stub_event_count("LOST") == 0, "assist lost frames must not mark target LOST");
    CHECK(stub_event_count("RELEASE") == 0, "assist lost frames must not release target");
    CHECK(stub_84e_n == n84, "no relock 84E request from assist frames");
    CHECK(stub_track_count(0) == 1, "primary TRACK still only once");
}

/* 3. 主板丢失后成功复锁 */
static void s3_relock_success(void)
{
    boot();
    uint8_t id = acquire(-30.0f, 250.0f, 0);
    feed(0, id, -30.0f + 4.6f, 0, 0.0f, 1500);
    feed(0, id, 0, 1, 0.0f, 700);              /* 丢失 0.7 s */
    CHECK(stub_event_count("LOST") == 1, "LOST event expected");
    feed(0, id, -30.0f + 4.6f, 0, 0.0f, 600);
    CHECK(stub_event_count("RELOCK") == 1, "RELOCK event expected after recovery");
    CHECK(stub_event_count("RELEASE") == 0, "target must not be released");
}

/* 4. 一直找不回：重试上限后释放，两块板都释放 */
static void s4_relock_gives_up(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    feed(0, id, 5.71f, 0, 0.0f, 1500);
    int rel_before = stub_release_n;
    feed(0, id, 0, 1, 0.0f, 5000);
    long reason = -1;
    CHECK(stub_event_count("RELEASE") == 1, "RELEASE expected after retries, got %d", stub_event_count("RELEASE"));
    stub_event_last("RELEASE", &reason, NULL, NULL);
    CHECK(reason == 1, "release reason should be 1 (relock failed), got %ld", reason);
    CHECK(stub_release_n - rel_before >= 2, "both primary and assist boards released");
    /* 复锁期间只允许主板被重新瞄准，不得两块板都发 TRACK */
    int relock_tracks_b = 0;
    for (int i = 0; i < stub_track_n; i++) if (stub_track[i].esp == 1) relock_tracks_b++;
    CHECK(relock_tracks_b <= 1, "relock must not hijack board B (got %d TRACK)", relock_tracks_b);
}

/* 5. 84E 判为非人体：并入雷达背景、移除、短时内同一位置不再送识别 */
static void s5_reject_non_human(void)
{
    boot();
    stub_push_candidate(40.0f, 180.0f, 0.9f);
    int k = wait_84e(0, 3000);
    CHECK(k == 0, "84E command expected");
    stub_cb84e(stub_84e[0].id, 0.05f, 0, 0, 0, 0);
    run_ms(50);
    CHECK(stub_absorb_n == 1, "rejected position must be absorbed into radar background");
    CHECK_NEAR(stub_absorb_last, 40.0, 1.0, "absorbed bearing");
    stub_push_candidate(41.0f, 185.0f, 0.9f);
    run_ms(2000);
    CHECK(stub_84e_n == 1, "same spot must not be re-sent to 84E (got %d commands)", stub_84e_n);
    CHECK(stub_track_n == 0, "no ESP32 assignment for a non-human");
}

/* 5b. 判定阈值与 84E 一致：0.3~0.5 之间按非人体处理，恰好 0.5 按人体处理 */
static void s5b_human_threshold(void)
{
    boot();
    stub_push_candidate(40.0f, 180.0f, 0.9f);
    CHECK(wait_84e(0, 3000) == 0, "84E command expected");
    stub_cb84e(stub_84e[0].id, 0.4f, 0, 0, 0, 0);
    run_ms(2000);
    CHECK(stub_absorb_n == 1, "conf 0.4 is background on the 84E, H7 must reject it too");
    CHECK(stub_track_n == 0, "no ESP32 assignment for conf 0.4");

    stub_push_candidate(-40.0f, 180.0f, 0.9f);
    int k = wait_84e(1, 3000);
    CHECK(k == 1, "second 84E command expected");
    if (k == 1) stub_cb84e(stub_84e[1].id, 0.5f, 0, 0, 0, 0);
    run_ms(2000);
    CHECK(stub_absorb_n == 1, "conf 0.5 counts as human");
    CHECK(stub_track_n >= 1, "conf 0.5 gets an ESP32");
}

/* 5c. 板间通信统计定期输出，三个通道各一行 */
static void s5c_comm_stats_event(void)
{
    boot();
    run_ms(5200);
    CHECK(stub_event_count("COMM") == 3, "one COMM event per channel, got %d", stub_event_count("COMM"));
    long frames = 0, errs = 0, other = 0;
    CHECK(stub_event_last("COMM", &frames, &errs, &other), "COMM event readable");
    CHECK(frames == 102 && errs == 1, "COMM carries the counters: %ld/%ld", frames, errs);
}

/* 6. 三个目标：两块板各跟一个，第三个上报；有板空出后补上 */
static void s6_three_targets(void)
{
    boot();
    stub_push_candidate(-60.0f, 220.0f, 0.9f);
    stub_push_candidate(60.0f, 240.0f, 0.9f);
    stub_push_candidate(0.0f, 300.0f, 0.9f);
    uint8_t ids[3] = {0};
    for (int i = 0; i < 3; i++) {
        int k = wait_84e(i, 4000);
        CHECK(k == i, "84E command %d expected", i);
        if (k < 0) return;
        ids[i] = stub_84e[k].id;
        stub_cb84e(ids[i], 0.9f, 0, 0, 0, 0);
    }
    run_ms(3000);
    CHECK(stub_track_count(0) >= 1 && stub_track_count(1) >= 1, "both boards assigned");
    CHECK(stub_event_count("THIRD") == 1, "third target reported once, got %d", stub_event_count("THIRD"));
    CHECK(stub_text_n >= 3, "LAN/voice texts sent for third target");

    /* 找出跟 -60° 目标的板，让它丢失直到释放 */
    uint8_t id_a = 0; int esp_a = -1;
    for (int i = 0; i < stub_track_n; i++) {
        if (fabsf(stub_track[i].angle) < 5.0f && (stub_track[i].id == ids[0] || stub_track[i].id == ids[1])) {
            if (stub_track[i].id == ids[0]) { id_a = ids[0]; esp_a = stub_track[i].esp; }
        }
    }
    CHECK(esp_a >= 0, "board tracking the first target identified");
    if (esp_a < 0) return;
    int before = stub_track_count((uint8_t)esp_a);
    for (int t = 0; t < 6000; t += 33) {
        stub_cbesp((uint8_t)esp_a, id_a, 0, 0, 0, 0, 1);
        stub_cbesp((uint8_t)(1 - esp_a), ids[1], esp_x_for(60.0f, gimbal_bearing(1 - esp_a)), 112.5f, 0, 0, 0);
        run_ms(33);
    }
    CHECK(stub_event_count("RELEASE") >= 1, "lost target released");
    int third_assigned = 0;
    for (int i = 0; i < stub_track_n; i++) if (stub_track[i].id == ids[2]) third_assigned = 1;
    CHECK(third_assigned, "third target picked up by the freed board");
    (void)before;
}

/* 7. STOP：释放两板、全部回位、忽略回传；START 后能重新发现 */
static void s7_stop_start(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    feed(0, id, 5.71f, 0, 0.0f, 1000);
    int rel = stub_release_n;
    Scheduler_RequestShutdown();
    CHECK(Scheduler_IsPaused(), "paused after STOP");
    CHECK(stub_release_n - rel >= 2, "both boards released on STOP");
    int tracks = stub_track_n;
    feed(0, id, 5.71f, 0, 0.0f, 3000);
    CHECK(stub_track_n == tracks, "no TRACK while stopped");
    CHECK(Scheduler_IsShutdownComplete(), "homing completes");
    for (int g = 0; g < 3; g++) CHECK_NEAR(Gimbal_GetEstimatedPan((GimbalId_t)g), 135.0, 0.5, "gimbal homed");
    Scheduler_Resume();
    CHECK(!Scheduler_IsPaused(), "running after START");
    int n = stub_84e_n;
    stub_push_candidate(-20.0f, 150.0f, 0.9f);
    CHECK(wait_84e(n, 3000) >= 0, "new candidate handled after START");
}

/* 8. 84E 不回复：超时后继续下一个候选 */
static void s8_84e_timeout(void)
{
    boot();
    stub_push_candidate(-50.0f, 200.0f, 0.9f);
    stub_push_candidate(50.0f, 260.0f, 0.9f);
    CHECK(wait_84e(0, 3000) == 0, "first 84E command");
    uint32_t t0 = stub_tick;
    int k = wait_84e(1, 5000);
    CHECK(k == 1, "second candidate dispatched after timeout");
    CHECK(stub_event_count("TO84E") == 1, "TO84E event");
    CHECK(stub_tick - t0 >= 1500, "not before the 1.5 s timeout (%lu ms)", (unsigned long)(stub_tick - t0));
}

/* 9. 边界接力：主板云台推到 255° 时交给另一块板 */
static void s9_handover(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    feed(0, id, 5.71f, 0, 0.0f, 600);
    int b_before = stub_track_count(1);
    int rel_a = stub_release_count(0);
    /* 正控制量：每帧 +2°，从 225° 推到 255° 约需 15 帧 */
    for (int i = 0; i < 40 && stub_event_count("HOSTART") == 0; i++) {
        stub_cbesp(0, id, 150.0f, 112.5f, 400.0f, 0, 0);
        run_ms(33);
    }
    CHECK(stub_event_count("HOSTART") == 1, "handover started at the pan limit");
    CHECK(wait_track(1, b_before, 3000), "board B receives TRACK for handover");
    for (int i = 0; i < 10; i++) {
        stub_cbesp(1, id, 150.0f, 112.5f, 0, 0, 0);
        stub_cbesp(0, id, 150.0f, 112.5f, 0, 0, 0);
        run_ms(33);
    }
    CHECK(stub_event_count("HOOK") == 1, "handover completes when B locks");
    CHECK(stub_release_count(0) > rel_a, "board A released after handover");
}

typedef void (*Scenario_t)(void);
static const struct { const char *name; Scenario_t fn; } k_scenarios[] = {
    { "single_target",      s1_single_target },
    { "assist_lost",        s2_assist_lost_harmless },
    { "relock_success",     s3_relock_success },
    { "relock_gives_up",    s4_relock_gives_up },
    { "reject_non_human",   s5_reject_non_human },
    { "human_threshold",    s5b_human_threshold },
    { "comm_stats_event",   s5c_comm_stats_event },
    { "three_targets",      s6_three_targets },
    { "stop_start",         s7_stop_start },
    { "84e_timeout",        s8_84e_timeout },
    { "handover",           s9_handover },
};
#define N_SCEN ((int)(sizeof(k_scenarios) / sizeof(k_scenarios[0])))

int main(int argc, char **argv)
{
    if (getenv("TEST_VERBOSE")) stub_verbose = 1;
    if (argc >= 2) {
        int n = atoi(argv[1]);
        if (n < 0 || n >= N_SCEN) return 2;
        printf("[scheduler] %s\n", k_scenarios[n].name);
        k_scenarios[n].fn();
        TEST_REPORT(k_scenarios[n].name);
    }
    int failed = 0;
    for (int i = 0; i < N_SCEN; i++) {
        char cmd[512];
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "\"\"%s\" %d\"", argv[0], i);   /* cmd.exe 会剥掉最外层引号 */
#else
        snprintf(cmd, sizeof(cmd), "\"%s\" %d", argv[0], i);
#endif
        if (system(cmd) != 0) failed++;
    }
    printf("scheduler scenarios: %d/%d passed\n", N_SCEN - failed, N_SCEN);
    return failed ? 1 : 0;
}
