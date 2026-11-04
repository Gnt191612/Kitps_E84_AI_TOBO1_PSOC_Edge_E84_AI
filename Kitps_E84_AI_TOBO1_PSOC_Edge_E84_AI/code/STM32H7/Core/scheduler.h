/**
 * @file    scheduler.h
 * @brief   多进程轮转调度器
 *
 * 公开接口：
 *   Scheduler_Init()  - 初始化所有子系统、注册通信回调、启动进程
 *   Scheduler_Run()   - 主循环入口，执行当前进程并响应对应事件
 */

#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROC_RADAR_SCAN = 0,      // 雷达扫描进程
    PROC_84E_RECOG,           // 等待84E识别结果
    PROC_ESP32_TRACK,         // ESP32跟踪中
    PROC_IDLE                 // 空闲
} Process_t;

void Scheduler_Init(void);
void Scheduler_Run(void);
void Scheduler_RequestShutdown(void);   /* 停止：释放全部目标，雷达与云台回初始朝向 */
uint8_t Scheduler_IsShutdownComplete(void);
uint8_t Scheduler_IsPaused(void);         /* 已请求停止（含回位过程中） */
void Scheduler_Resume(void);              /* 从停止状态恢复，雷达背景保留 */
void Scheduler_PrintStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* __SCHEDULER_H */
