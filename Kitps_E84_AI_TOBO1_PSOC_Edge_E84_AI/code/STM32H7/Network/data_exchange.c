/**
 * @file    data_exchange.c
 * @brief   向上位机推送数据的格式化与发送
 *
 * 通过 ESP32-A 的 WiFi 中继（UART→WebSocket）转发到手机端。
 * 不再直连 TCP，移除不可行的 eth/tcp 方案。
 *
 * 协议：简单文本行，以 \r\n 结束。
 *  目标列表: "TARGETS: id1:x,y; id2:x,y; \r\n"
 *  切换指令: "HANDOVER: target_id at distance_cm\r\n"
 *  统计报告: "STATS: recog_rate=xx%, loss_rate=xx%\r\n"
 */

#include "data_exchange.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>

/* 默认通过 ESP32-A（esp_id=0）转发 */
#define UPLOAD_ESP_ID   0

void DataExchange_Init(void)
{
    /* 无需额外初始化，依赖 protocol.c 已就绪 */
}

void DataExchange_SendTargetList(TargetList_t *list)
{
    char buf[256];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "TARGETS: ");
    for (int i = 0; i < list->count; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%d:%.1f,%.1f; ",
                        list->targets[i].id,
                        list->targets[i].x,
                        list->targets[i].y);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\r\n");
    Protocol_SendESP32Text(UPLOAD_ESP_ID, buf, pos);
}

void DataExchange_SendHandover(uint8_t target_id, float distance_cm)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "HANDOVER: %d at %.1f cm\r\n",
                       target_id, distance_cm);
    Protocol_SendESP32Text(UPLOAD_ESP_ID, buf, len);
}

void DataExchange_SendStats(float recog_error_rate, float loss_rate)
{
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
                       "STATS: recog_error=%.1f%%, loss_rate=%.1f%%\r\n",
                       recog_error_rate * 100.0f, loss_rate * 100.0f);
    Protocol_SendESP32Text(UPLOAD_ESP_ID, buf, len);
}
