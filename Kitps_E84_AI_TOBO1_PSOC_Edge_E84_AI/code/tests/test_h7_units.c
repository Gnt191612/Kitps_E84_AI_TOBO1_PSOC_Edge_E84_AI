/*
 * H7 单元测试：雷达距离图（背景学习/变化检测）、超声波脉宽换算、三角定位与平面坐标。
 */
#include "test.h"
#include "main.h"
#include "radar_map.h"
#include "radar_driver.h"
#include "geometry.h"
#include <stdlib.h>
#include <string.h>

TEST_MAIN_DEFS

/* 替身全局量 */
TIM_TypeDef stub_TIM2, stub_TIM3;
GPIO_TypeDef stub_GPIOA, stub_GPIOB, stub_GPIOC, stub_GPIOD;
TIM_HandleTypeDef htim2 = { &stub_TIM2, 0 }, htim3 = { &stub_TIM3, 0 }, htim6;
UART_HandleTypeDef huart1, huart2, huart3, huart4;
uint32_t stub_tick = 1000;
uint32_t stub_capture_value = 0;
void stub_set_compare(TIM_HandleTypeDef *h, uint32_t ch, uint32_t v) { (void)h; (void)ch; (void)v; }
void Error_Handler(void) { abort(); }

/* ---------------- 模拟房间 ---------------- */
/* 舵机角 0~270° 上的背景距离：墙 300 cm，60~75° 有柜子 120 cm，200~210° 是门（无回波） */
static float room_range(float servo_deg)
{
    if (servo_deg >= 200.0f && servo_deg <= 210.0f) return -1.0f;
    if (servo_deg >= 60.0f && servo_deg <= 75.0f) return 120.0f;
    return 300.0f;
}

typedef struct { float servo_deg, half_width_deg, dist_cm; int present; } Person_t;

static float noise_cm(void) { return (float)((rand() % 61) - 30) / 10.0f; }   /* ±3 cm */

/* 模拟一个往复扫描周期（4 段，每段 3 s，每 70 ms 一次测量）；返回产生的候选数，
 * 最后一个候选写入 *last */
static int sweep_cycle(const Person_t *p, TargetCandidate_t *last)
{
    int ncand = 0;
    const float leg_ms = 3000.0f;
    for (int leg = 0; leg < 4; leg++) {
        for (float t = 0; t < leg_ms; t += 70.0f) {
            float prog = t / leg_ms;
            float a = (leg == 0) ? 135.0f * (1 - prog) :
                      (leg == 1) ? 135.0f * prog :
                      (leg == 2) ? 135.0f + 135.0f * prog : 270.0f - 135.0f * prog;
            float d = room_range(a);
            if (p && p->present && fabsf(a - p->servo_deg) <= p->half_width_deg) d = p->dist_cm;
            if (d > 0) d += noise_cm();
            stub_tick += 70;
            int bin = RadarMap_AddMeasurement(a, d, stub_tick);
            TargetCandidate_t c;
            if (bin >= 0 && d > 0 && RadarMap_BackgroundReady() &&
                RadarMap_CandidateAt(bin, stub_tick, &c)) {
                ncand++;
                if (last) *last = c;
            }
        }
    }
    return ncand;
}

static void test_radar_map(void)
{
    printf("[radar_map]\n");
    srand(1);
    RadarMap_Init();
    int learn_cycles = 0;
    while (!RadarMap_BackgroundReady() && learn_cycles < 10) { sweep_cycle(NULL, NULL); learn_cycles++; }
    CHECK(RadarMap_BackgroundReady(), "background should be learned");
    CHECK(learn_cycles <= 3, "learning should take <= 3 cycles (36 s), took %d", learn_cycles);

    /* 空房间（含柜子、门、±3 cm 噪声）：不得产生候选 */
    int n = sweep_cycle(NULL, NULL) + sweep_cycle(NULL, NULL);
    CHECK(n == 0, "static room must give no candidates, got %d", n);

    /* 人站在舵机 165°（方位 +30°）、150 cm，约占 ±6° */
    Person_t p = { 165.0f, 6.0f, 150.0f, 1 };
    TargetCandidate_t c; memset(&c, 0, sizeof(c));
    n = sweep_cycle(&p, &c);
    CHECK(n > 0, "person should be detected");
    CHECK_NEAR(c.angle_deg, 30.0, 5.0, "candidate bearing");
    CHECK_NEAR(c.distance_cm, 150.0, 5.0, "candidate distance");

    /* 人站在柜子前（柜子 120 cm，人 80 cm）也要能发现 */
    Person_t p2 = { 67.0f, 4.0f, 80.0f, 1 };
    memset(&c, 0, sizeof(c));
    n = sweep_cycle(&p2, &c);
    CHECK(n > 0, "person in front of furniture detected");
    CHECK_NEAR(c.angle_deg, 67.0 - 135.0, 6.0, "bearing in front of furniture");

    /* 人站在门口（背景无回波） */
    Person_t p3 = { 205.0f, 5.0f, 250.0f, 1 };
    memset(&c, 0, sizeof(c));
    n = sweep_cycle(&p3, &c);
    CHECK(n > 0, "person in the doorway (no-echo background) detected");

    /* 人离开后不再报 */
    sweep_cycle(NULL, NULL);
    n = sweep_cycle(NULL, NULL);
    CHECK(n == 0, "no candidates after the person leaves, got %d", n);

    /* 新放的椅子被 84E 判为非人体 → 并入背景后不再报 */
    Person_t chair = { 100.0f, 5.0f, 180.0f, 1 };
    n = sweep_cycle(&chair, &c);
    CHECK(n > 0, "new chair first shows up as a change");
    RadarMap_Absorb(c.angle_deg, 10.0f);
    sweep_cycle(&chair, NULL);
    n = sweep_cycle(&chair, NULL);
    CHECK(n == 0, "absorbed chair must not be reported again, got %d", n);

    /* 单个随机短读数（噪声尖峰）不得成为候选 */
    RadarMap_Init();
    for (int i = 0; i < 3; i++) sweep_cycle(NULL, NULL);
    stub_tick += 70;
    int bin = RadarMap_AddMeasurement(240.0f, 90.0f, stub_tick);
    CHECK(!RadarMap_CandidateAt(bin, stub_tick, &c), "single spike must not be a candidate");

    /* 量程边缘：背景无回波，墙在 3.9 m 左右回波时有时无，不得报成目标；3 m 处的人仍要发现 */
    RadarMap_Init();
    for (int i = 0; i < 3; i++) { stub_tick += 70; RadarMap_AddMeasurement(200.0f, -1.0f, stub_tick); }
    CHECK_NEAR(RadarMap_BackgroundAt(200.0f), RADAR_FAR_CM, 0.01, "no-echo background");
    for (int i = 0; i < 3; i++) {
        stub_tick += 70;
        bin = RadarMap_AddMeasurement(200.0f, 390.0f, stub_tick);
        CHECK(!RadarMap_CandidateAt(bin, stub_tick, &c), "echo flicker near max range must not be a candidate");
    }
    for (int i = 0; i < 2; i++) { stub_tick += 70; bin = RadarMap_AddMeasurement(200.0f, 300.0f, stub_tick); }
    CHECK(RadarMap_CandidateAt(bin, stub_tick, &c), "person at 3 m in a no-echo direction is still found");
}

/* ---------------- 超声波脉宽 ---------------- */
static float measure(uint32_t rise_cap, uint32_t width_us)
{
    stub_tick += 100;
    CHECK(Radar_Trigger() == RADAR_OK, "trigger accepted");
    htim3.Channel = HAL_TIM_ACTIVE_CHANNEL_1;
    stub_capture_value = rise_cap;
    stub_tick += 1;
    HAL_TIM_IC_CaptureCallback(&htim3);
    uint32_t fall = (rise_cap + width_us) % 20000U;
    stub_tick += width_us / 1000U;
    stub_capture_value = fall;
    HAL_TIM_IC_CaptureCallback(&htim3);
    float d = -1;
    RadarResult_t r = Radar_Poll(&d);
    return (r == RADAR_RESULT_OK) ? d : -1.0f;
}

static void test_radar_driver(void)
{
    printf("[radar_driver]\n");
    Radar_Driver_Init();
    CHECK_NEAR(measure(1000, 5831), 100.0, 0.2, "1 m, no wrap");
    CHECK_NEAR(measure(19000, 11662), 200.0, 0.3, "2 m, wraps once");
    CHECK_NEAR(measure(19990, 22000), 377.3, 0.5, "3.77 m, echo longer than the 20 ms timer period");
    CHECK(measure(500, 25000) < 0, "beyond 4 m must not become a point");

    /* 无回波：超时后给出 NO_ECHO 且可再次触发 */
    stub_tick += 100;
    CHECK(Radar_Trigger() == RADAR_OK, "trigger");
    CHECK(Radar_Trigger() == RADAR_BUSY, "second trigger while in flight is refused");
    float d;
    stub_tick += 20;
    CHECK(Radar_Poll(&d) == RADAR_RESULT_NONE, "still waiting at 20 ms");
    stub_tick += 30;
    CHECK(Radar_Poll(&d) == RADAR_RESULT_NO_ECHO, "timeout -> NO_ECHO");
    stub_tick += 100;
    CHECK(Radar_Trigger() == RADAR_OK, "can trigger again after timeout");
}

/* ---------------- 几何 ---------------- */
static void test_geometry(void)
{
    printf("[geometry]\n");
    float x, y;
    Geometry_PolarToPlane(0, 0, 0, 100, &x, &y);
    CHECK_NEAR(x, 0, 1e-3, "bearing 0 -> +Y (x)"); CHECK_NEAR(y, 100, 1e-3, "bearing 0 -> +Y (y)");
    Geometry_PolarToPlane(0, 0, 90, 100, &x, &y);
    CHECK_NEAR(x, 100, 1e-3, "bearing +90 -> +X");
    float b, d;
    Geometry_PlaneToPolar(0, 20, 30, 70, &b, &d);
    CHECK_NEAR(b, 30.964, 0.01, "plane->polar bearing"); CHECK_NEAR(d, 58.31, 0.01, "plane->polar dist");

    /* 目标 (35, 260)：分别从 A(-20,0)、B(20,0) 求方位再三角定位，应还原目标 */
    float ba, bb, da, db;
    Geometry_PlaneToPolar(-20, 0, 35, 260, &ba, &da);
    Geometry_PlaneToPolar(20, 0, 35, 260, &bb, &db);
    CHECK(Geometry_Triangulate(-20, 0, ba, 20, 0, bb, &x, &y), "triangulation valid");
    CHECK_NEAR(x, 35, 0.05, "tri x"); CHECK_NEAR(y, 260, 0.05, "tri y");
    CHECK(!Geometry_Triangulate(-20, 0, 10, 20, 0, 10, &x, &y), "parallel rays rejected");
    /* 两条射线向外张开（A 偏左、B 偏右），延长线交于相机背后 → 无效 */
    CHECK(!Geometry_Triangulate(-20, 0, -10, 20, 0, 10, &x, &y), "intersection behind cameras rejected");
}

int main(void)
{
    test_radar_map();
    test_radar_driver();
    test_geometry();
    TEST_REPORT("h7_units");
}
