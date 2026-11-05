/*
 * 3D 仿真的固件宿主：把真实的 H7 调度器、雷达、云台和目标管理代码编译成 WebAssembly，
 * 硬件相关的部分（HAL、串口帧、超声波测距）换成对 JavaScript 的调用，由 sim_3d/world.js 提供物理世界。
 *
 * 用真实代码的范围和 tests/run_tests.ps1 的 test_h7_demo_flow 相同。
 * 串口帧的打包解析不在这里（tests/test_protocol.c 单独测），命令和结果以函数调用传递。
 */
#include "main.h"
#include "logger.h"
#include "radar_driver.h"
#include "radar_process.h"
#include "scheduler.h"
#include "gimbal.h"
#include "servo.h"
#include "protocol.h"
#include "cmd_84e.h"
#include "cmd_esp32.h"
#include "data_exchange.h"
#include "network.h"
#include "delay_record.h"
#include "console.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define JS_IMPORT(name) __attribute__((import_module("env"), import_name(name)))
#define JS_EXPORT(name) __attribute__((export_name(name)))

JS_IMPORT("js_log")        void  js_log(const char *text, int len);
JS_IMPORT("js_pwm")        void  js_pwm(int tim, int channel, int pulse_us);
JS_IMPORT("js_radar_ping") float js_radar_ping(void);
JS_IMPORT("js_cmd_84e")    void  js_cmd_84e(int id, float angle, float width, float dmin, float dmax);
JS_IMPORT("js_cmd_track")  void  js_cmd_track(int esp, float angle, float dist, int id);
JS_IMPORT("js_cmd_release") void js_cmd_release(int esp);
JS_IMPORT("js_abort")      void  js_abort(void);

/* ---- HAL ---- */
TIM_TypeDef stub_TIM2, stub_TIM3;
GPIO_TypeDef stub_GPIOA, stub_GPIOB, stub_GPIOC, stub_GPIOD;
TIM_HandleTypeDef htim2 = { &stub_TIM2, 0 }, htim3 = { &stub_TIM3, 0 }, htim6;
UART_HandleTypeDef huart1, huart2, huart3, huart4;
uint32_t stub_tick = 1;
uint32_t stub_capture_value;

void stub_set_compare(TIM_HandleTypeDef *h, uint32_t ch, uint32_t v)
{
    js_pwm(h == &htim2 ? 2 : 3, (int)(ch / 4U) + 1, (int)v);
}

void Error_Handler(void) { js_abort(); }

/* ---- 日志 ---- */
static void emit_line(const char *format, va_list ap)
{
    char line[160];
    int n = vsnprintf(line, sizeof(line), format, ap);
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    js_log(line, n);
}

void Logger_Init(void) {}
void Logger_SetLevel(LogLevel_t level) { (void)level; }
void Logger_Print(LogLevel_t level, const char *format, ...)
{
    (void)level;
    va_list a; va_start(a, format);
    emit_line(format, a);
    va_end(a);
}
void Logger_Raw(const char *format, ...)
{
    va_list a; va_start(a, format);
    emit_line(format, a);
    va_end(a);
}

/* ---- 超声波：时序和 tests/test_h7_demo_flow.c 的替身相同，距离由物理世界算 ---- */
static int s_ping_busy;
static uint32_t s_ping_done;
static float s_ping_cm;

void Radar_Driver_Init(void) { s_ping_busy = 0; }
RadarStatus_t Radar_Trigger(void)
{
    if (s_ping_busy) return RADAR_BUSY;
    s_ping_cm = js_radar_ping();
    s_ping_done = stub_tick + (s_ping_cm < 0.0f ? RADAR_ECHO_TIMEOUT_MS
                                                : (uint32_t)(s_ping_cm * 58.0f / 1000.0f) + 1U);
    s_ping_busy = 1;
    return RADAR_OK;
}
RadarResult_t Radar_Poll(float *dist_cm)
{
    if (!s_ping_busy || stub_tick < s_ping_done) return RADAR_RESULT_NONE;
    s_ping_busy = 0;
    if (s_ping_cm < 0.0f) return RADAR_RESULT_NO_ECHO;
    *dist_cm = s_ping_cm;
    return RADAR_RESULT_OK;
}

/* ---- 协议与命令 ---- */
static Recv84ECallback_t s_cb84e;
static RecvESP32Callback_t s_cbesp;
static Protocol_CommStats_t s_stats[PROTO_CH_COUNT];

void Protocol_Init(void) { memset(s_stats, 0, sizeof(s_stats)); }
void Protocol_Register84ECallback(Recv84ECallback_t cb) { s_cb84e = cb; }
void Protocol_RegisterESP32Callback(RecvESP32Callback_t cb) { s_cbesp = cb; }
void Protocol_ProcessIncoming(void) {}
void Protocol_GetCommStats(uint8_t ch, Protocol_CommStats_t *out)
{
    if (ch < PROTO_CH_COUNT) *out = s_stats[ch]; else memset(out, 0, sizeof(*out));
}
void Protocol_SendESP32Text(uint8_t esp_id, const char *text, uint16_t len)
{ (void)esp_id; (void)text; (void)len; }

void Cmd_84E_Init(void) {}
void Cmd_84E_SendScanCmd(uint8_t target_id, float center_angle, float width,
                         float min_dist, float max_dist)
{ js_cmd_84e(target_id, center_angle, width, min_dist, max_dist); }

void Cmd_ESP32_Init(void) {}
void Cmd_ESP32_SendTrackCmd(uint8_t esp_id, float angle, float distance, uint8_t target_id)
{ js_cmd_track(esp_id, angle, distance, target_id); }
void Cmd_ESP32_SendReleaseCmd(uint8_t esp_id) { js_cmd_release(esp_id); }

/* ---- 不影响调度逻辑的模块 ---- */
void DataExchange_Init(void) {}
void DataExchange_SendTargetList(TargetList_t *list) { (void)list; }
void DataExchange_SendHandover(uint8_t target_id, float distance_cm) { (void)target_id; (void)distance_cm; }
void Network_SendTargetHandover(uint8_t target_id, float distance_cm) { (void)target_id; (void)distance_cm; }
void DelayRecord_Start84E(uint8_t target_id) { (void)target_id; }
void DelayRecord_Stop84E(uint8_t target_id) { (void)target_id; }
void Console_Init(void) {}
void Console_Poll(void) {}

/* ---- 给 JavaScript 用的接口 ---- */
JS_EXPORT("fw_init") void fw_init(void)
{
    Servo_Init();
    Gimbal_Init();
    Radar_Process_Init();
    Scheduler_Init();
}
/* 推进 1 ms 并执行一轮主循环 */
JS_EXPORT("fw_step") void fw_step(void)
{
    stub_tick++;
    Scheduler_Run();
}
JS_EXPORT("fw_tick") uint32_t fw_tick(void) { return stub_tick; }
JS_EXPORT("fw_on_84e") void fw_on_84e(int id, float conf, float dist, float angle)
{
    s_stats[PROTO_CH_84E].frames++;
    if (s_cb84e) s_cb84e((uint8_t)id, conf, 0.0f, 0.0f, dist, angle);
}
JS_EXPORT("fw_on_esp") void fw_on_esp(int esp, int id, float x, float y, float pan, float tilt, int lost)
{
    s_stats[esp == 0 ? PROTO_CH_ESP32_A : PROTO_CH_ESP32_B].frames++;
    if (s_cbesp) s_cbesp((uint8_t)esp, (uint8_t)id, x, y, pan, tilt, (uint8_t)lost);
}
/* 串口干扰造成的校验失败帧，只计数 */
JS_EXPORT("fw_note_bad") void fw_note_bad(int ch)
{
    if (ch >= 0 && ch < (int)PROTO_CH_COUNT) s_stats[ch].xor_errors++;
}
JS_EXPORT("fw_stop") void fw_stop(void) { Scheduler_RequestShutdown(); }
JS_EXPORT("fw_start") void fw_start(void) { Scheduler_Resume(); }
JS_EXPORT("fw_relearn") void fw_relearn(void) { Scheduler_RelearnBackground(); }
JS_EXPORT("fw_status") void fw_status(void) { Scheduler_PrintStatus(); }
JS_EXPORT("fw_bg_ready") int fw_bg_ready(void) { return Radar_Process_BackgroundReady(); }
JS_EXPORT("fw_est_pan") float fw_est_pan(int g) { return Gimbal_GetEstimatedPan((GimbalId_t)g); }
JS_EXPORT("fw_est_servo") float fw_est_servo(void) { return Servo_GetAngle(); }
