/*
 * H7 调度器场景测试：真实 scheduler.c + 云台/舵机/目标列表/复锁/融合/预测/实验记录，
 * 只把 HAL、雷达测量、串口收发换成替身（stubs/h7_stubs.c）。
 *
 * 编译两份：EXP_RECOG_WHILE_TRACK=0 的 test_h7_scheduler 跑基本流程（84E 单帧回复，由场景逐条控制），
 * EXP_RECOG_WHILE_TRACK=1 的 test_h7_recog_track 跑 84E 边扫描边跟踪。
 *
 * 用法：test_h7_scheduler        依次在子进程里跑全部场景
 *       test_h7_scheduler <n>    只跑第 n 个场景（调度器内部状态不可重置，故每个场景单独进程）
 */
#include "test.h"
#include "h7_stubs.h"
#include "scheduler.h"
#include "gimbal.h"
#include "config_mount.h"
#include "exp_config.h"
#include <math.h>
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
    int before = stub_track_count((uint8_t)expect_esp);
    stub_push_candidate(bearing, dist, 0.9f);
    int k = wait_84e(n, 3000);
    CHECK(k >= 0, "radar candidate should trigger an 84E command");
    if (k < 0) return 0;
    uint8_t id = stub_84e[k].id;
    CHECK_NEAR(stub_84e[k].angle, bearing, 2.0, "84E aimed bearing");
    stub_cb84e(id, 0.9f, 0, 0, dist, bearing);
    CHECK(wait_track((uint8_t)expect_esp, before, 4000), "ESP32-%c should get a TRACK command", 'A' + expect_esp);
    return id;
}

/* 连续回复 n 次同一置信度（每次都等 H7 发出下一条命令）；first 为第一条命令的下标 */
static int answer_84e(int first, int n, float conf)
{
    for (int i = 0; i < n; i++) {
        if (wait_84e(first + i, 3000) != first + i) return 0;
        stub_cb84e(stub_84e[first + i].id, conf, 0, 0, 0, 0);
    }
    return 1;
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
    CHECK(answer_84e(0, 3, 0.05f), "84E asked three times before giving up");
    run_ms(50);
    CHECK(stub_absorb_n == 1, "rejected position must be absorbed into radar background");
    CHECK_NEAR(stub_absorb_last, 40.0, 1.0, "absorbed bearing");
    stub_push_candidate(41.0f, 185.0f, 0.9f);
    run_ms(2000);
    CHECK(stub_84e_n == 3, "same spot must not be re-sent to 84E (got %d commands)", stub_84e_n);
    CHECK(stub_track_n == 0, "no ESP32 assignment for a non-human");
}

/* 5b. 判定阈值与 84E 一致：0.3~0.5 之间按非人体处理，恰好 0.5 按人体处理 */
static void s5b_human_threshold(void)
{
    boot();
    stub_push_candidate(40.0f, 180.0f, 0.9f);
    CHECK(answer_84e(0, 3, 0.4f), "conf 0.4 asked three times");
    run_ms(2000);
    CHECK(stub_absorb_n == 1, "conf 0.4 is background on the 84E, H7 must reject it too");
    CHECK(stub_track_n == 0, "no ESP32 assignment for conf 0.4");

    stub_push_candidate(-40.0f, 180.0f, 0.9f);
    CHECK(answer_84e(3, 1, 0.5f), "next candidate asked");
    run_ms(2000);
    CHECK(stub_absorb_n == 1, "conf 0.5 counts as human");
    CHECK(stub_track_n >= 1, "conf 0.5 gets an ESP32");
}

/* 5d. 一两次漏判不能把人记成背景 */
static void s5d_miss_then_human(void)
{
    boot();
    stub_push_candidate(40.0f, 180.0f, 0.9f);
    CHECK(answer_84e(0, 2, 0.10f), "asked again after a miss");
    run_ms(100);
    CHECK(stub_absorb_n == 0, "two misses must not absorb the position");
    CHECK(stub_event_count("RELEASE") == 0, "candidate kept while unconfirmed");
    CHECK(answer_84e(2, 1, 0.90f), "third look");
    run_ms(2000);
    CHECK(stub_absorb_n == 0, "confirmed human is not absorbed");
    CHECK(stub_track_n >= 1, "confirmed human gets an ESP32");
}

/* 5e. 跟踪中的人走近或走远：雷达距离更新到目标上，不当成新目标；紧挨着的另一距离才算新目标 */
static void s5e_radial_walk(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    feed(0, id, 5.71f, 0, 0.0f, 3000);
    int n84 = stub_84e_n;
    stub_push_candidate(0.0f, 320.0f, 0.9f);
    feed(0, id, 3.6f, 0, 0.0f, 1000);
    CHECK(stub_84e_n == n84, "same person walking away must not become a new target (%d new commands)", stub_84e_n - n84);
    long x = 0, y = 0, src = 0;
    CHECK(stub_event_last("FUSE", &x, &y, &src), "FUSE event");
    CHECK(y > 280, "fused position should follow the new distance, y=%ld", y);

    /* 1 s 前刚更新过距离，范围约 180 cm；220 cm 之外的回波只能是另一个人 */
    stub_push_candidate(0.0f, 100.0f, 0.9f);
    CHECK(wait_84e(n84, 500) >= 0, "a different distance right after an update is a new target");
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

/* 10. 主板一直无回传：重发一次后改派 B；A 不再被用作辅助；A 回传后恢复可用 */
static void s10_failover_never_replied(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    run_ms(2500);
    CHECK(stub_track_count(0) == 2, "resend once before giving up, got %d", stub_track_count(0));
    CHECK(stub_event_count("FAILOVER") == 0, "not moved before the resend had a chance");
    int rel_a = stub_release_count(0);
    int b_before = stub_track_count(1);
    CHECK(wait_track(1, b_before, 3000), "board B gets TRACK as the new primary");
    CHECK(stub_event_count("FAILOVER") == 1, "FAILOVER event, got %d", stub_event_count("FAILOVER"));
    CHECK(stub_release_count(0) > rel_a, "board A released");

    feed(1, id, -5.71f, 0, 0.0f, 3000);
    long esp = -1;
    CHECK(stub_event_count("LOCK") == 1, "B locks the target");
    stub_event_last("LOCK", &esp, NULL, NULL);
    CHECK(esp == 1, "LOCK reported by board B, got %ld", esp);
    int a_tracks = stub_track_count(0);
    feed(1, id, -5.71f, 0, 0.0f, 3000);
    CHECK(stub_track_count(0) == a_tracks, "silent board A is not used as assist");
    CHECK(stub_event_count("FAILOVER") == 1, "stays on B while B replies");

    /* A 又回传了（比如复位后恢复）：H7 让它释放，之后可以再做辅助 */
    stub_cbesp(0, id, 150.0f, 112.5f, 0, 0, 0);
    feed(1, id, -5.71f, 0, 0.0f, 3000);        /* A 从初始朝向转过去要 1 s 多 */
    CHECK(stub_track_count(0) > a_tracks, "board A is an assist again once it replies");
}

/* 11. 跟踪中才断：回传停止后改派；断开期间帧又回来就不改派 */
static void s11_failover_after_lock(void)
{
    boot();
    uint8_t id = acquire(0.0f, 200.0f, 0);
    feed(0, id, 5.71f, 0, 0.0f, 1000);
    run_ms(1500);                               /* 短暂中断，触发一次重发 */
    feed(0, id, 5.71f, 0, 0.0f, 3000);
    CHECK(stub_event_count("FAILOVER") == 0, "no failover when the board comes back");

    feed(0, id, 0, 1, 0.0f, 700);               /* 丢失后才断：目标不能卡在 LOST */
    CHECK(stub_event_count("LOST") == 1, "LOST before the link dies");
    run_ms(5000);
    CHECK(stub_event_count("FAILOVER") == 1, "lost target moved too, got %d", stub_event_count("FAILOVER"));
    feed(1, id, -5.71f, 0, 0.0f, 500);
    CHECK(stub_event_count("RELOCK") == 1, "new primary recovers the lost target");
}

/* 12. 另一块板在跟别的目标：不抢，继续重发；它空出来后再改派 */
static void s12_failover_needs_free_board(void)
{
    boot();
    stub_push_candidate(-30.0f, 220.0f, 0.9f);
    stub_push_candidate(30.0f, 240.0f, 0.9f);
    uint8_t ids[2] = {0};
    for (int i = 0; i < 2; i++) {
        CHECK(wait_84e(i, 4000) == i, "84E command %d expected", i);
        ids[i] = stub_84e[i].id;
        stub_cb84e(ids[i], 0.9f, 0, 0, 0, 0);
    }
    run_ms(2000);
    int silent = -1;
    for (int i = stub_track_n - 1; i >= 0; i--) if (stub_track[i].id == ids[0]) silent = stub_track[i].esp;
    CHECK(silent >= 0, "first target assigned");
    if (silent < 0) return;
    int busy = 1 - silent;
    for (int t = 0; t < 6000; t += 33) {        /* 只有 busy 板回传 */
        stub_cbesp((uint8_t)busy, ids[1], esp_x_for(30.0f, gimbal_bearing(busy)), 112.5f, 0, 0, 0);
        run_ms(33);
    }
    CHECK(stub_event_count("FAILOVER") == 0, "a busy board must not be taken over");
    CHECK(stub_track_count((uint8_t)silent) >= 3, "keep resending to the silent board");
    for (int t = 0; t < 6000; t += 33) {        /* busy 板跟丢，目标释放后板子空出来 */
        stub_cbesp((uint8_t)busy, ids[1], 0, 0, 0, 0, 1);
        run_ms(33);
    }
    run_ms(2500);
    CHECK(stub_event_count("FAILOVER") == 1, "moved once a board is free, got %d", stub_event_count("FAILOVER"));
}

/* 13. 两块板都不回传：不来回切换 */
static void s13_failover_both_silent(void)
{
    boot();
    acquire(0.0f, 200.0f, 0);
    run_ms(20000);
    CHECK(stub_event_count("FAILOVER") == 1, "only one move when both boards are silent, got %d",
          stub_event_count("FAILOVER"));
}

/* 14. 人离得近时雷达和主板的方位差很多，还是同一个人，不能算新目标；辅助板也要对得准 */
static void s14_near_person_geometry(void)
{
    boot();
    /* 真实位置 (-87, 104)：相对 A -32.8°，相对雷达 (0,20) -46.0°。雷达波束宽，还可能报到 -60° */
    uint8_t id = acquire(-33.0f, 108.0f, 0);
    feed(0, id, -32.8f, 0, 0.0f, 3000);
    /* 相对 B -46°，B 板转不到（范围从 -30° 起），不该去当辅助 */
    CHECK(stub_track_count(1) == 0, "ESP32-B cannot cover this bearing, got %d TRACK", stub_track_count(1));

    int n84 = stub_84e_n;
    stub_push_candidate(-46.0f, 108.0f, 0.9f);
    run_ms(200);
    stub_push_candidate(-62.5f, 112.0f, 0.9f);   /* 典型波束下实际报过的值 */
    run_ms(200);
    CHECK(stub_event_count("CAND") == 1, "same person must not become a second target, got %d", stub_event_count("CAND"));
    /* 同样距离上，真正离得远的另一个人仍是新目标 */
    stub_push_candidate(40.0f, 112.0f, 0.9f);
    run_ms(200);
    CHECK(stub_event_count("CAND") == 2, "a person far away is a new target, got %d", stub_event_count("CAND"));
    CHECK(stub_84e_n == n84, "no new 84E request for the same person");
}

/* 15. 近处辅助板要按自己的位置瞄准；看不到时过一会儿重新瞄 */
static void s15_near_assist_aim(void)
{
    boot();
    /* 真实位置 (0, 105)：相对 A +10.8°，相对 B -10.8° */
    uint8_t id = acquire(10.8f, 107.0f, 0);
    feed(0, id, 10.8f, 0, 0.0f, 3000);
    CHECK(stub_track_count(1) == 1, "assist board should start");
    CHECK_NEAR(gimbal_bearing(1), -10.8, 2.0, "assist board must aim from where it stands");
    CHECK(fabs(stub_track[stub_track_n - 1].angle) < 2.0f, "person should be near the middle of the assist image");

    /* 辅助板一直看不到：主板不受影响，辅助板定期重新瞄准 */
    int before = stub_track_count(1);
    for (int i = 0; i < 100; i++) {
        stub_cbesp(0, id, esp_x_for(10.8f, gimbal_bearing(0)), 112.5f, 0, 0, 0);
        stub_cbesp(1, id, 0, 0, 0, 0, 1);
        run_ms(33);
    }
    CHECK(stub_track_count(1) > before, "assist should re-aim after losing the target");
    CHECK(stub_track_count(1) <= before + 5, "assist re-aim must be rate limited, got %d", stub_track_count(1) - before);
    CHECK(stub_event_count("LOST") == 0, "assist losing sight must not mark the target lost");
}

#if EXP_RECOG_WHILE_TRACK
/* ---------- 84E 边扫描边跟踪 ---------- */
#define IMG_X(off_deg)  (80.0f + (off_deg) * (160.0f / 50.0f))

/* 发现目标，等 84E 云台到位、收到开始扫描的命令；返回目标 id */
static uint8_t scan_start(float bearing, float dist)
{
    int n = stub_84e_n;
    stub_push_candidate(bearing, dist, 0.9f);
    int k = wait_84e(n, 3000);
    CHECK(k >= 0, "84E should be told to start scanning");
    return (k >= 0) ? stub_84e[k].id : 0;
}

/* 84E 回传一帧：目标真实方位 b，按云台当前朝向算它在画面中的位置 */
static void scan_frame(uint8_t id, float conf, float b)
{
    float off = b - gimbal_bearing(2);
    float x = (off > -25.0f && off < 25.0f) ? IMG_X(off) : -1.0f;
    stub_cb84e(id, conf, x, 60.0f, 0, 0);
    run_ms(60);
}

/* 完整流程：雷达 → 84E 转过去 → 边扫描边跟踪 → 认定后才交给 ESP32，84E 待命 */
static void r1_full_flow(void)
{
    boot();
    uint8_t id = scan_start(-30.0f, 200.0f);
    CHECK_NEAR(gimbal_bearing(2), -30.0, 2.0, "84E gimbal turned to the radar bearing");
    CHECK(stub_event_count("ASSIGN") == 0, "no ESP32 before 84E recognizes");
    CHECK(stub_track_n == 0, "no TRACK before 84E recognizes");

    /* 目标在走，第一帧看到但还不够确信：84E 云台跟着转 */
    float b = -30.0f;
    for (int i = 0; i < 8; i++) { scan_frame(id, 0.3f, b); b += 1.0f; }
    CHECK(stub_track_n == 0, "still no ESP32 while unsure");
    CHECK(fabs(gimbal_bearing(2) - b) < 3.0, "84E gimbal should follow: %.1f vs %.1f", gimbal_bearing(2), b);
    CHECK(stub_84e_stop_n == 0, "84E keeps scanning");

    /* 连续三帧判为人：认定 */
    scan_frame(id, 0.9f, b); b += 1.0f;
    scan_frame(id, 0.9f, b); b += 1.0f;
    CHECK(stub_track_n == 0, "two frames are not enough");
    scan_frame(id, 0.9f, b);
    CHECK(stub_84e_stop_n == 1, "84E told to stop after recognizing");
    CHECK(stub_event_count("ASSIGN") >= 1, "ESP32 assigned after recognizing");
    CHECK(stub_event_count("RES84E") == 1, "one conclusion");

    /* 交给 ESP32 的坐标：人还在走，瞄的是它转到位时的位置 */
    CHECK(wait_track(0, 0, 3000), "ESP32 should get TRACK");
    float gb = gimbal_bearing(0);
    CHECK(gb > b - 8.0f && gb < b + 20.0f, "ESP32 should aim ahead of the last seen bearing: %.1f, last %.1f", gb, b);

    /* ESP32 锁定后 84E 停在原处待命，不再发命令 */
    float pan84 = gimbal_bearing(2);
    int n84 = stub_84e_n;
    feed(0, id, gb, 0, 0.0f, 6000);
    CHECK(stub_event_count("LOCK") == 1, "LOCK");
    CHECK(stub_84e_n == n84, "84E stays idle after handing over");
    CHECK_NEAR(gimbal_bearing(2), pan84, 0.5, "84E gimbal holds its position");
}

/* 单帧误报不算：判为人必须连续 */
static void r2_single_false_positive(void)
{
    boot();
    uint8_t id = scan_start(-30.0f, 200.0f);
    for (int i = 0; i < 4; i++) {
        scan_frame(id, 0.9f, -30.0f);
        scan_frame(id, 0.1f, -30.0f);
    }
    CHECK(stub_track_n == 0, "scattered false positives must not start ESP32");
}

/* 物体：累计多帧不是人就放弃，从头到尾 ESP32 不动 */
static void r3_object_rejected(void)
{
    boot();
    uint8_t id = scan_start(-30.0f, 200.0f);
    for (int i = 0; i < 10; i++) scan_frame(id, 0.1f, -30.0f);
    run_ms(10);
    CHECK(stub_event_count("RELEASE") == 1, "released after enough rejecting frames");
    long reason = -1;
    stub_event_last("RELEASE", &reason, NULL, NULL);
    CHECK(reason == 0, "release reason: not human");
    CHECK(stub_absorb_n == 1, "object absorbed into background");
    CHECK(stub_84e_stop_n == 1, "84E stopped");
    CHECK(stub_event_count("ASSIGN") == 0, "ESP32 never moved");
    CHECK(stub_track_n == 0, "ESP32 never told to track");
}

/* 84E 看不到人也没有结论：到时限放弃，不能一直占着 84E */
static void r4_scan_time_limit(void)
{
    boot();
    uint8_t id = scan_start(-30.0f, 200.0f);
    for (int i = 0; i < 60 && stub_84e_stop_n == 0; i++) {
        /* 判为人和不是人交替，永远凑不够连续帧，也攒得很慢 */
        scan_frame(id, (i % 12 == 0) ? 0.1f : ((i % 3 == 0) ? 0.1f : 0.9f), -30.0f);
        run_ms(40);
    }
    CHECK(stub_84e_stop_n == 1, "scan must end");
}

/* 84E 不回传：超时放弃并发停止命令 */
static void r5_84e_silent(void)
{
    boot();
    (void)scan_start(-30.0f, 200.0f);
    run_ms(2000);
    CHECK(stub_event_count("TO84E") == 1, "timeout expected");
    CHECK(stub_84e_stop_n == 1, "stop sent on timeout");
    CHECK(stub_track_n == 0, "no ESP32");
}

/* 只要还在回传就不算超时 */
static void r6_frames_keep_alive(void)
{
    boot();
    uint8_t id = scan_start(-30.0f, 200.0f);
    for (int i = 0; i < 40; i++) { stub_cb84e(id, 0.3f + (i % 2 ? 0.25f : 0.0f), IMG_X(0), 60.0f, 0, 0); run_ms(60); if (stub_84e_stop_n) break; }
    CHECK(stub_event_count("TO84E") == 0, "no timeout while frames arrive");
}

/* 扫描中雷达又扫到同一个人：不是新目标 */
static void r7_radar_during_scan(void)
{
    boot();
    uint8_t id = scan_start(-30.0f, 200.0f);
    float b = -30.0f;
    for (int i = 0; i < 6; i++) { scan_frame(id, 0.3f, b); b += 2.0f; }
    int n = stub_84e_n;
    stub_push_candidate(b + 12.0f, 190.0f, 0.9f);
    run_ms(100);
    CHECK(stub_event_count("CAND") == 1, "radar echo of the same person is not a new target, got %d", stub_event_count("CAND"));
    CHECK(stub_84e_n == n, "no second 84E command");
    /* 方位还是 84E 看到的，不被雷达改掉 */
    scan_frame(id, 0.9f, b); scan_frame(id, 0.9f, b); scan_frame(id, 0.9f, b);
    CHECK(wait_track(0, 0, 3000), "ESP32 gets TRACK");
    /* 人在走（33°/s），A 板要转约 0.8 s，瞄的是那时的位置；雷达报的 b+12 没有参与 */
    CHECK(gimbal_bearing(0) > b && gimbal_bearing(0) < b + 30.0f, "ESP32 should aim ahead of 84E's bearing, %.1f vs %.1f", gimbal_bearing(0), b);
}

/* 84E 云台还在转向时雷达又扫到同一个人：也不是新目标（否则两块板会跟同一个人，接力没板可用） */
static void r11_radar_during_turn(void)
{
    boot();
    stub_push_candidate(-45.0f, 245.0f, 0.9f);
    run_ms(100);
    CHECK(stub_84e_n == 0, "84E gimbal is still turning");
    stub_push_candidate(-62.0f, 211.0f, 0.9f);
    run_ms(100);
    CHECK(stub_event_count("CAND") == 1, "same person while 84E turns, got %d targets", stub_event_count("CAND"));
    /* 真正离得远的另一个人还是新候选 */
    stub_push_candidate(60.0f, 200.0f, 0.9f);
    run_ms(100);
    CHECK(stub_event_count("CAND") == 2, "a person far away is new, got %d", stub_event_count("CAND"));
}

/* 第二个候选等第一个结束后再扫 */
static void r8_second_candidate_waits(void)
{
    boot();
    uint8_t id = scan_start(-40.0f, 200.0f);
    stub_push_candidate(60.0f, 180.0f, 0.9f);
    run_ms(100);
    CHECK(stub_84e_n == 1, "only one scan at a time");
    int n = stub_84e_n;
    for (int i = 0; i < 3; i++) scan_frame(id, 0.9f, -40.0f);
    int k = wait_84e(n, 3000);
    CHECK(k >= 0 && stub_84e[k].id != id, "next candidate scanned after the first one concludes");
    if (k >= 0) CHECK_NEAR(gimbal_bearing(2), 60.0, 3.0, "84E turned to the next candidate");
}

/* 刚接手的跟踪板转到位后还在找目标：几帧没看到不算跟丢，一直找不到才算 */
static void r10_acquire_grace(void)
{
    boot();
    uint8_t id = scan_start(0.0f, 200.0f);
    for (int i = 0; i < 3; i++) scan_frame(id, 0.9f, 0.0f);
    CHECK(wait_track(0, 0, 4000), "ESP32 gets TRACK");
    feed(0, id, 0.0f, 1, 0.0f, 300);
    CHECK(stub_event_count("LOST") == 0, "not lost while still searching");
    feed(0, id, 0.0f, 1, 0.0f, 400);
    CHECK(stub_event_count("LOST") == 1, "lost when it never finds the target");
}

/* 跟丢时 84E 再扫：只有跟踪板重新看到才算复锁，84E 给它新坐标 */
static void r9_relock_scan(void)
{
    boot();
    uint8_t id = scan_start(0.0f, 200.0f);
    for (int i = 0; i < 3; i++) scan_frame(id, 0.9f, 0.0f);
    CHECK(wait_track(0, 0, 4000), "ESP32 gets TRACK");
    feed(0, id, 5.7f, 0, 0.0f, 2000);
    int n = stub_84e_n, tracks = stub_track_count(0);
    feed(0, id, 0.0f, 1, 0.0f, 100);
    CHECK(stub_event_count("LOST") == 1, "LOST");
    int k = wait_84e(n, 4000);
    CHECK(k >= 0, "84E should scan again");
    if (k < 0) return;
    for (int i = 0; i < 3; i++) scan_frame(id, 0.9f, 20.0f);
    CHECK(stub_event_count("RELOCK") == 0, "84E alone is not a relock");
    CHECK(wait_track(0, tracks, 3000), "tracking board told where to look");
}
#endif

typedef void (*Scenario_t)(void);
static const struct { const char *name; Scenario_t fn; } k_scenarios[] = {
#if EXP_RECOG_WHILE_TRACK
    { "full_flow",          r1_full_flow },
    { "single_false_positive", r2_single_false_positive },
    { "object_rejected",    r3_object_rejected },
    { "scan_time_limit",    r4_scan_time_limit },
    { "84e_silent",         r5_84e_silent },
    { "frames_keep_alive",  r6_frames_keep_alive },
    { "radar_during_scan",  r7_radar_during_scan },
    { "second_candidate_waits", r8_second_candidate_waits },
    { "relock_scan",        r9_relock_scan },
    { "acquire_grace",      r10_acquire_grace },
    { "radar_during_turn",  r11_radar_during_turn },
#else
    { "single_target",      s1_single_target },
    { "assist_lost",        s2_assist_lost_harmless },
    { "relock_success",     s3_relock_success },
    { "relock_gives_up",    s4_relock_gives_up },
    { "reject_non_human",   s5_reject_non_human },
    { "human_threshold",    s5b_human_threshold },
    { "miss_then_human",    s5d_miss_then_human },
    { "radial_walk",        s5e_radial_walk },
    { "comm_stats_event",   s5c_comm_stats_event },
    { "three_targets",      s6_three_targets },
    { "stop_start",         s7_stop_start },
    { "84e_timeout",        s8_84e_timeout },
    { "handover",           s9_handover },
    { "failover_silent",    s10_failover_never_replied },
    { "failover_after_lock", s11_failover_after_lock },
    { "failover_busy_board", s12_failover_needs_free_board },
    { "failover_both_silent", s13_failover_both_silent },
    { "near_person_geometry", s14_near_person_geometry },
    { "near_assist_aim",    s15_near_assist_aim },
#endif
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
