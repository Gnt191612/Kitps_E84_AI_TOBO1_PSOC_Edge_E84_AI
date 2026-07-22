/**
 * @file    scheduler.h
 * @brief   简易任务调度器（模拟三种进程轮转）
 *
 * 公开接口：
 *   Scheduler_Init()      - 初始化所有算法模块、协议解析、模型
 *   Scheduler_Run()       - 主循环中每一次调用，执行当前进程
 *   Scheduler_PushCommand() - 接收 H7 命令的回调（由 protocol 调用）
 */

#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include <stdint.h>
#include "protocol.h"   // CommandPacket_t, ResultPacket_t

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROC_IDLE = 0,
    PROC_AUX_SCAN,
    PROC_TARGET_RECOG
} Process_t;

void Scheduler_Init(void);
void Scheduler_Run(void);
void Scheduler_PushCommand(CommandPacket_t *cmd);   // H7 命令到达回调

/* ---- J5 中断引脚回调声明（在 system.c 中注册到 Comm） ---- */
void Scheduler_Int0Callback(void);   // (未使用, BB UART TX)
void Scheduler_Int1Callback(void);   // (未使用, BB UART RX)
void Scheduler_Int2Callback(void);   // INT2 备用
void Scheduler_Int3Callback(void);   // INT3 备用

#ifdef __cplusplus
}
#endif

#endif /* __SCHEDULER_H */