/*
 * ESP32 跟踪算法测试：合成 160×120 灰度画面（纹理背景 + 移动的纹理目标），
 * 用真实的 tracker.c / track_algorithm.c / roi_select.c 跟踪，检查误差、丢失与复锁。
 * 同一份源码分别以 EXP_TRACK_FULL_FRAME=0/1 编译，对应对比实验①。
 */
#include "test.h"
#include "tracking/tracker.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

TEST_MAIN_DEFS

#define W 160
#define H 120
#define PX_PER_MM (1.0f / 1.875f)

void Logger_Print(int level, const char *fmt, ...) { (void)level; (void)fmt; }

/* 模拟时钟：每帧 33 ms（与板上 30 fps 一致），供 track_algorithm.c 的卡尔曼 dt 使用 */
static uint32_t g_sim_ms = 1000;
uint32_t test_time_ms(void) { return g_sim_ms; }

static uint8_t g_bg[W * H];
static uint8_t g_tex[120 * 120];   /* 目标自身纹理（目标坐标系） */

static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static void make_background(int strong)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int v;
            if (strong) {
                /* 强纹理：8 px 随机灰度方块（模拟货架、墙纸） */
                srand((unsigned)((x / 8) * 131 + (y / 8) * 977 + 7));
                v = 60 + rand() % 140;
            } else {
                /* 弱纹理：平滑起伏 + 少量噪声（模拟墙面） */
                v = (int)(110 + 20 * sinf(x / 9.0f) + 15 * cosf(y / 7.0f));
                srand((unsigned)(x * 7919 + y * 104729));
                v += rand() % 9 - 4;
            }
            g_bg[y * W + x] = clamp8(v);
        }
    srand(12345);
    for (int i = 0; i < 120 * 120; i++) g_tex[i] = 0;
    for (int by = 0; by < 20; by++)
        for (int bx = 0; bx < 20; bx++) {
            int v = rand() % 256;
            for (int y = 0; y < 6; y++)
                for (int x = 0; x < 6; x++) g_tex[(by * 6 + y) * 120 + bx * 6 + x] = (uint8_t)v;
        }
}

/* 目标中心 (cx,cy)、尺寸 tw×th；visible=0 表示被遮挡 */
static void render(uint8_t *f, float cx, float cy, int tw, int th, int visible, unsigned seed)
{
    g_sim_ms += 33;   /* 每渲染一帧推进 33 ms */
    memcpy(f, g_bg, sizeof(g_bg));
    if (visible) {
        int x0 = (int)lroundf(cx) - tw / 2, y0 = (int)lroundf(cy) - th / 2;
        for (int y = 0; y < th; y++)
            for (int x = 0; x < tw; x++) {
                int px = x0 + x, py = y0 + y;
                if (px < 0 || py < 0 || px >= W || py >= H) continue;
                f[py * W + px] = g_tex[(y % 120) * 120 + (x % 120)];
            }
    }
    srand(seed);
    for (int i = 0; i < W * H; i++) f[i] = clamp8(f[i] + rand() % 7 - 3);   /* 传感器噪声 */
}

static float tracked_px(Tracker_t *t) { return t->result_x_mm * PX_PER_MM; }
static float tracked_py(Tracker_t *t) { return t->result_y_mm * PX_PER_MM; }

/* 目标在画面中左右来回移动（模拟云台还没跟上），返回最大误差 */
static float run_motion(int strong_bg, int tw, int th, double *ms_per_frame)
{
    static uint8_t frame[W * H];
    static Tracker_t tr;
    make_background(strong_bg);
    memset(&tr, 0, sizeof(tr));
    Tracker_Init(&tr, 1, W, H);
    float cx = 80, cy = 60;
    Tracker_StartAt(&tr, (int)cx, (int)cy, 0);
    float max_err = 0, max_raw = 0;
    int lost_frames = 0;
    clock_t c0 = clock();
    for (int i = 0; i < 90; i++) {
        float v = (i < 30) ? 1.5f : (i < 60) ? -1.5f : 1.0f;
        cx += v;
        cy += (i % 20 < 10) ? 0.3f : -0.3f;
        render(frame, cx, cy, tw, th, 1, (unsigned)i);
        Tracker_Process(&tr, 1, frame);
        if (tr.lost_flag) { lost_frames++; continue; }
        if (i >= 3) {   /* 前几帧卡尔曼收敛 */
            float e = hypotf(tracked_px(&tr) - cx, tracked_py(&tr) - cy);
            if (e > max_err) max_err = e;
            /* 匹配本身给出的中心（滤波前），用于区分匹配误差和滤波滞后 */
            float r = hypotf(tr.algorithm.cx - cx, tr.algorithm.cy - cy);
            if (r > max_raw) max_raw = r;
        }
    }
    *ms_per_frame = 1000.0 * (double)(clock() - c0) / CLOCKS_PER_SEC / 90.0;
    printf("    (match-only error %.2f px)\n", max_raw);
    CHECK(max_raw <= 3.5f, "template-matching error %.2f px", max_raw);
    CHECK(lost_frames == 0, "target must not be lost while visible (lost %d frames)", lost_frames);
    return max_err;
}

/* 目标静止：输出不应乱抖（否则 PID 会让云台来回抖动） */
static void test_stationary_jitter(void)
{
    static uint8_t frame[W * H];
    static Tracker_t tr;
    make_background(0);
    memset(&tr, 0, sizeof(tr));
    Tracker_Init(&tr, 1, W, H);
    Tracker_StartAt(&tr, 80, 60, 0);
    double sx = 0, sxx = 0; int n = 0;
    for (int i = 0; i < 80; i++) {
        render(frame, 80, 60, 36, 80, 1, 300 + i);
        Tracker_Process(&tr, 1, frame);
        if (i >= 10) { double x = tracked_px(&tr); sx += x; sxx += x * x; n++; }
    }
    double sd = sqrt(sxx / n - (sx / n) * (sx / n));
    printf("  stationary target: output std %.3f px\n", sd);
    CHECK(sd <= 0.5, "stationary output jitter %.3f px", sd);
    CHECK_NEAR(sx / n, 80.0, 2.0, "stationary mean x");
}

static void test_occlusion(void)
{
    static uint8_t frame[W * H];
    static Tracker_t tr;
    make_background(0);
    memset(&tr, 0, sizeof(tr));
    Tracker_Init(&tr, 1, W, H);
    Tracker_StartAt(&tr, 80, 60, 0);
    for (int i = 0; i < 10; i++) { render(frame, 80, 60, 36, 70, 1, i); Tracker_Process(&tr, 1, frame); }
    for (int i = 0; i < 10; i++) { render(frame, 80, 60, 36, 70, 0, 100 + i); Tracker_Process(&tr, 1, frame); }
    CHECK(tr.state == TRACK_STATE_LOST, "occluded target -> LOST (state %d)", tr.state);
    /* 重新出现，位置偏了 12 px：用目标自身模板找回 */
    Tracker_StartAt(&tr, 80, 60, 1);   /* H7 复锁时会再次下发同一目标 */
    int ok_frame = -1;
    for (int i = 0; i < 6; i++) {
        render(frame, 92, 62, 36, 70, 1, 200 + i);
        Tracker_Process(&tr, 1, frame);
        if (!tr.lost_flag && ok_frame < 0) ok_frame = i;
    }
    CHECK(ok_frame >= 0 && ok_frame <= 2, "reacquired within 3 frames (frame %d)", ok_frame);
    CHECK_NEAR(tracked_px(&tr), 92, 4.0, "reacquired x (px)");
}

int main(void)
{
    double ms;
    printf("[esp32_tracker] mode=%s\n", EXP_TRACK_FULL_FRAME ? "full-frame" : "roi3");
    float e = run_motion(0, 36, 80, &ms);
    printf("  weak background, 36x80 target: max error %.2f px, %.3f ms/frame (PC)\n", e, ms);
    CHECK(e <= 4.0f, "tracking error on weak background %.2f px", e);

    e = run_motion(1, 36, 80, &ms);
    printf("  strong background, 36x80 target: max error %.2f px, %.3f ms/frame (PC)\n", e, ms);
    CHECK(e <= 4.0f, "tracking error on strong background %.2f px", e);

    e = run_motion(0, 20, 44, &ms);
    printf("  weak background, small 20x44 target (far person): max error %.2f px\n", e);
    CHECK(e <= 4.0f, "tracking error for small target %.2f px", e);

    test_stationary_jitter();
    test_occlusion();
    TEST_REPORT(EXP_TRACK_FULL_FRAME ? "esp32_tracker_full" : "esp32_tracker_roi3");
}
