/* H7 调度器测试：替身记录的数据，供测试检查 */
#ifndef H7_STUBS_H
#define H7_STUBS_H

#include <stdint.h>
#include "protocol.h"
#include "target_candidate.h"

#define STUB_MAX_REC 256

typedef struct { uint8_t id; float angle, width, dmin, dmax; uint32_t tick; } Rec84E_t;
typedef struct { uint8_t esp, id; float angle, dist; uint32_t tick; } RecTrack_t;
typedef struct { uint8_t esp; uint32_t tick; } RecRelease_t;

extern Rec84E_t     stub_84e[STUB_MAX_REC];      extern int stub_84e_n;
extern RecTrack_t   stub_track[STUB_MAX_REC];    extern int stub_track_n;
extern RecRelease_t stub_release[STUB_MAX_REC];  extern int stub_release_n;
extern int   stub_absorb_n;   extern float stub_absorb_last;
extern int   stub_text_n;     /* 发往 ESP32-A 的文本帧数（第三目标上报） */
extern int   stub_verbose;

extern Recv84ECallback_t   stub_cb84e;
extern RecvESP32Callback_t stub_cbesp;

void stub_reset(void);
void stub_push_candidate(float bearing_deg, float dist_cm, float conf);
/* 实验事件计数：统计 Logger_Raw 中 "$E,...,<name>," 出现次数 */
int  stub_event_count(const char *name);
/* 最近一条某事件的 v1/v2/v3，找不到返回 0 */
int  stub_event_last(const char *name, long *v1, long *v2, long *v3);

int  stub_track_count(uint8_t esp);
int  stub_release_count(uint8_t esp);

#endif
