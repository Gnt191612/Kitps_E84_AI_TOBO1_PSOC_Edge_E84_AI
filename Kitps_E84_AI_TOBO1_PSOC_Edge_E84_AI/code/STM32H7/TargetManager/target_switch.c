/**
 * @file    target_switch.c
 * @brief   第三目标处理：视觉跟踪资源已满时上报上位机 + 语音播报
 *
 * 调度器在“84E 已确认是人、但两块 ESP32 都在跟踪别的目标”时调用一次。
 * 上报内容经 ESP32-A 的文本中继（帧类型 0x03）发往局域网客户端：
 *   HANDOVER / TARGETS / VOICE 三行文本。
 * 当前 ESP32 固件 ENABLE_EXTERNAL_CONTROL = 0，中继未启用，文本会被忽略；
 * 启用局域网后无需再改 H7。
 */

#include "target_switch.h"
#include "network.h"
#include "protocol.h"
#include "data_exchange.h"
#include <stdio.h>
#include <string.h>

void TargetSwitch_Execute(TargetList_t *list, uint8_t unassigned_id)
{
    if (!list) return;
    TrackedTarget_t *t = TargetList_GetTarget(list, unassigned_id);
    if (!t) return;

    /* ─── 1. 通知上位机：该目标需要由客户端切换跟踪 ─── */
    Network_SendTargetHandover(t->id, t->distance_cm);

    /* ─── 2. 上报完整目标列表 ─── */
    DataExchange_SendTargetList(list);

    /* ─── 3. 语音播报（客户端解析后播放） ─── */
    char voice_buf[96];
    int vlen = snprintf(voice_buf, sizeof(voice_buf),
                        "VOICE:third_target_%d,dist=%.0fcm,angle=%.0f",
                        t->id, t->distance_cm, t->angle_deg);
    if (vlen > 0) {
        if (vlen >= (int)sizeof(voice_buf)) vlen = (int)sizeof(voice_buf) - 1;
        Protocol_SendESP32Text(0, voice_buf, (uint16_t)vlen);
    }
}
