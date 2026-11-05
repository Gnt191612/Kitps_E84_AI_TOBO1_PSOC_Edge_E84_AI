/* H7 调度器测试替身：HAL、日志、雷达、协议、命令下发 */
#include "main.h"
#include "h7_stubs.h"
#include "logger.h"
#include "radar_process.h"
#include "cmd_84e.h"
#include "cmd_esp32.h"
#include "data_exchange.h"
#include "network.h"
#include "delay_record.h"
#include "console.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* ---- HAL ---- */
TIM_TypeDef stub_TIM2, stub_TIM3;
GPIO_TypeDef stub_GPIOA, stub_GPIOB, stub_GPIOC, stub_GPIOD;
TIM_HandleTypeDef htim2 = { &stub_TIM2, 0 }, htim3 = { &stub_TIM3, 0 }, htim6;
UART_HandleTypeDef huart1, huart2, huart3, huart4;
uint32_t stub_tick = 1;
void stub_set_compare(TIM_HandleTypeDef *h, uint32_t ch, uint32_t v) { (void)h; (void)ch; (void)v; }
void Error_Handler(void) { fprintf(stderr, "Error_Handler called\n"); abort(); }

/* ---- 记录 ---- */
Rec84E_t     stub_84e[STUB_MAX_REC];      int stub_84e_n;
RecTrack_t   stub_track[STUB_MAX_REC];    int stub_track_n;
RecRelease_t stub_release[STUB_MAX_REC];  int stub_release_n;
int   stub_absorb_n;   float stub_absorb_last;
int   stub_text_n;
int   stub_verbose = 0;
Recv84ECallback_t   stub_cb84e;
RecvESP32Callback_t stub_cbesp;

#define LOG_LINES 4096
static char s_log[LOG_LINES][96];
static int  s_log_n;

static TargetCandidate_t s_cand[16];
static int s_cand_head, s_cand_n;

void stub_reset(void)
{
    stub_84e_n = stub_track_n = stub_release_n = 0;
    stub_absorb_n = 0; stub_text_n = 0;
    s_log_n = 0; s_cand_head = s_cand_n = 0;
}

void stub_push_candidate(float bearing_deg, float dist_cm, float conf)
{
    TargetCandidate_t c;
    memset(&c, 0, sizeof(c));
    c.angle_deg = bearing_deg;
    c.distance_cm = dist_cm;
    c.confidence = conf;
    c.valid = 1;
    s_cand[(s_cand_head + s_cand_n) % 16] = c;
    s_cand_n++;
}

int stub_event_count(const char *name)
{
    char key[32];
    snprintf(key, sizeof(key), ",%s,", name);
    int n = 0;
    for (int i = 0; i < s_log_n; i++) if (strstr(s_log[i], key)) n++;
    return n;
}

int stub_event_last(const char *name, long *v1, long *v2, long *v3)
{
    char key[32];
    snprintf(key, sizeof(key), ",%s,", name);
    for (int i = s_log_n - 1; i >= 0; i--) {
        char *p = strstr(s_log[i], key);
        if (!p) continue;
        unsigned id; long a, b, c;
        if (sscanf(p + strlen(key), "%u,%ld,%ld,%ld", &id, &a, &b, &c) == 4) {
            if (v1) *v1 = a;
            if (v2) *v2 = b;
            if (v3) *v3 = c;
            return 1;
        }
    }
    return 0;
}

int stub_track_count(uint8_t esp)
{
    int n = 0;
    for (int i = 0; i < stub_track_n; i++) if (stub_track[i].esp == esp) n++;
    return n;
}

int stub_release_count(uint8_t esp)
{
    int n = 0;
    for (int i = 0; i < stub_release_n; i++) if (stub_release[i].esp == esp) n++;
    return n;
}

/* ---- Logger ---- */
void Logger_Init(void) {}
void Logger_SetLevel(LogLevel_t level) { (void)level; }
void Logger_Print(LogLevel_t level, const char *format, ...)
{
    (void)level;
    if (!stub_verbose) return;
    va_list a; va_start(a, format);
    printf("    [log %lu] ", (unsigned long)stub_tick);
    vprintf(format, a);
    printf("\n");
    va_end(a);
}
void Logger_Raw(const char *format, ...)
{
    va_list a; va_start(a, format);
    if (s_log_n < LOG_LINES) vsnprintf(s_log[s_log_n++], sizeof(s_log[0]), format, a);
    va_end(a);
    if (stub_verbose && s_log_n > 0) printf("    %s\n", s_log[s_log_n - 1]);
}

/* ---- 雷达 ---- */
void Radar_Process_Init(void) {}
uint8_t Radar_Process_BackgroundReady(void) { return 1; }
void Radar_Process_Absorb(float bearing_deg) { stub_absorb_n++; stub_absorb_last = bearing_deg; }
uint8_t Radar_Process_Update(struct TargetCandidate_s *out)
{
    if (s_cand_n == 0) return 0;
    *(TargetCandidate_t *)out = s_cand[s_cand_head];
    s_cand_head = (s_cand_head + 1) % 16;
    s_cand_n--;
    return 1;
}

/* ---- 协议 ---- */
void Protocol_Init(void) {}
void Protocol_Register84ECallback(Recv84ECallback_t cb) { stub_cb84e = cb; }
void Protocol_RegisterESP32Callback(RecvESP32Callback_t cb) { stub_cbesp = cb; }
void Protocol_ProcessIncoming(void) {}
void Protocol_SendESP32Text(uint8_t esp_id, const char *text, uint16_t len)
{ (void)esp_id; (void)text; (void)len; stub_text_n++; }

void Cmd_84E_Init(void) {}
void Cmd_84E_SendScanCmd(uint8_t target_id, float center_angle, float width,
                         float min_dist, float max_dist)
{
    if (stub_84e_n < STUB_MAX_REC) {
        Rec84E_t r = { target_id, center_angle, width, min_dist, max_dist, stub_tick };
        stub_84e[stub_84e_n++] = r;
    }
}

void Cmd_ESP32_Init(void) {}
void Cmd_ESP32_SendTrackCmd(uint8_t esp_id, float angle, float distance, uint8_t target_id)
{
    if (stub_track_n < STUB_MAX_REC) {
        RecTrack_t r = { esp_id, target_id, angle, distance, stub_tick };
        stub_track[stub_track_n++] = r;
    }
}
void Cmd_ESP32_SendReleaseCmd(uint8_t esp_id)
{
    if (stub_release_n < STUB_MAX_REC) {
        RecRelease_t r = { esp_id, stub_tick };
        stub_release[stub_release_n++] = r;
    }
}

/* ---- 其他 ---- */
void DataExchange_Init(void) {}
void DataExchange_SendTargetList(TargetList_t *list) { (void)list; stub_text_n++; }
void DataExchange_SendHandover(uint8_t target_id, float distance_cm) { (void)target_id; (void)distance_cm; }
void Network_SendTargetHandover(uint8_t target_id, float distance_cm) { (void)target_id; (void)distance_cm; stub_text_n++; }
void DelayRecord_Start84E(uint8_t target_id) { (void)target_id; }
void DelayRecord_Stop84E(uint8_t target_id) { (void)target_id; }
void Console_Init(void) {}
void Console_Poll(void) {}
