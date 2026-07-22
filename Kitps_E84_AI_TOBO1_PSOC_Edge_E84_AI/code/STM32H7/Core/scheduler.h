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

#ifdef __cplusplus
}
#endif

#endif /* __SCHEDULER_H */