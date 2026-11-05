/**
 * @file    exp_log.h
 * @brief   实验事件记录：在 UART4 输出机器可解析的 CSV 行
 *
 * 行格式：$E,<tick_ms>,<事件名>,<target_id>,<v1>,<v2>,<v3>
 *   v1..v3 为整数，含义见下表（角度、置信度等按标注放大为整数）。
 * 上位机用 tools/exp_stats.py 统计开题报告第九节的指标。
 *
 *   RAD      一次雷达测量     v1=舵机角×10 v2=距离cm(-1无回波) v3=该格背景cm(0未学好)
 *   BGOK     雷达背景学习完成
 *   RELEARN  开始重新学习背景
 *   GIM      三路云台水平角(5次/秒) v1=ESP32-A×10 v2=ESP32-B×10 v3=84E×10
 *   ESPFREE  ESP32 释放        v1=esp
 *   PAUSE    系统停止
 *   RESUME   系统恢复
 *   CAND     雷达新候选       v1=方位×10  v2=距离cm   v3=0
 *   REQ84E   发出84E识别命令  v1=方位×10  v2=窗宽×10  v3=是否预判(0/1)
 *   RES84E   收到84E结果      v1=置信度×1000 v2=命令→结果延迟ms v3=0
 *   TO84E    84E超时          v1=0 v2=0 v3=0
 *   ASSIGN   分配ESP32        v1=esp v2=角色(1主 2辅 3接手) v3=0
 *   LOCK     主板首次锁定     v1=esp v2=雷达发现→锁定ms v3=0
 *   LOST     主板报丢失       v1=esp v2=0 v3=0
 *   RELOCK   复锁成功         v1=esp v2=丢失→复锁ms v3=0
 *   RELEASE  释放目标         v1=原因(1复锁失败) v2=0 v3=0
 *   HOSTART  开始接力         v1=from v2=to v3=Pan×10
 *   HOOK     接力成功         v1=to v2=耗时ms v3=0
 *   HOFAIL   接力超时         v1=to v2=0 v3=0
 *   FUSE     融合坐标         v1=x cm v2=y cm v3=来源位
 *   TRI      三角定位         v1=x cm v2=y cm v3=三角距离与雷达距离之差cm
 *   FPS      ESP32回传帧率    v1=esp v2=帧/秒 v3=其中丢失帧数
 *   COMM     H7接收统计(累计，每5秒) target_id=通道(0=84E 1=ESP32-A 2=ESP32-B)
 *            v1=正确帧 v2=校验或长度错误 v3=队列丢弃+串口错误
 */
#ifndef __EXP_LOG_H
#define __EXP_LOG_H

#include <stdint.h>
#include "exp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if EXP_LOG_ENABLE
void ExpLog_Event(const char *event, uint8_t target_id,
                  int32_t v1, int32_t v2, int32_t v3);
#else
#define ExpLog_Event(event, target_id, v1, v2, v3) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __EXP_LOG_H */
