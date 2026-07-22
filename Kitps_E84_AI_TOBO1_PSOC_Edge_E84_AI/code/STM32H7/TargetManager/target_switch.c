/**
 * @file    target_switch.c
 * @brief   第三个目标出现 → 触发语音播报 + 局域网切换
 *
 * 当系统中检测到3个独立目标时：
 *   1. 选择距离最远的目标交给上位机处理（腾出本地资源）  
 *   2. 发送语音播报指令（通过 ESP32 WiFi 中继 → 客户端播放）
 *   3. 发送 LAN 切换通知给客户端
 */

#include "target_switch.h"
#include "network.h"
#include "protocol.h"
#include "data_exchange.h"
#include <stdio.h>
#include <string.h>

/* 语音播报命令格式 */
#define VOICE_PREFIX    "VOICE:"

void TargetSwitch_Execute(TargetList_t *list)
{
    if (!list || list->count < 3) return;

    /* 找出距离最远的目标 */
    float max_dist = 0.0f;
    uint8_t switch_id = 0;
    int switch_idx = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->targets[i].distance_cm > max_dist) {
            max_dist = list->targets[i].distance_cm;
            switch_id = list->targets[i].id;
            switch_idx = i;
        }
    }

    TrackedTarget_t *t = &list->targets[switch_idx];

    /* ─── 1. 通知上位机 ─── */
    Network_SendTargetHandover(switch_id, max_dist);

    /* ─── 2. 通过 WebSocket 上报完整信息 ─── */
    DataExchange_SendTargetList(list);

    /* ─── 3. 语音播报 ───
     * 通过 ESP32-A 的 UART→WiFi 中继，发送语音指令给客户端。
     * 客户端 APP 收到后解析并播放对应语音。
     */
    {
        char voice_buf[128];
        int vlen = snprintf(voice_buf, sizeof(voice_buf),
            "VOICE:handover_target_%d,dist=%.0fcm,angle=%.0f",
            switch_id, max_dist, t->angle_deg);
        Protocol_SendESP32Text(0, voice_buf, vlen);
    }

    /* ─── 4. 输出日志 ─── */
    /* Logger_Print 由调度器外层处理 */
}
