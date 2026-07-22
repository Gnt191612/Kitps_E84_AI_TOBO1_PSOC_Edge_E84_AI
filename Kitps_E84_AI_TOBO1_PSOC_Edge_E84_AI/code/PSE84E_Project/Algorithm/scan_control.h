/**
 * @file    scan_control.h
 * @brief   往复扫描与分区扫描控制
 * 
 * 公开接口：
 *   ScanControl_Init()          - 初始化扫描控制器
 *   ScanControl_SetMode()       - 设置扫描模式（全范围往复 / 窗口分区扫描）
 *   ScanControl_SetWindow()     - 设置限定扫描窗口（中心角、宽度）
 *   ScanControl_GetNextAngle()  - 获取下一个扫描角度（状态机驱动）
 *   ScanControl_Reset()         - 复位扫描状态
 */

#ifndef __SCAN_CONTROL_H
#define __SCAN_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 扫描模式 */
typedef enum {
    SCAN_MODE_FULL_RANGE = 0,   /* 全范围往复扫描（默认） */
    SCAN_MODE_WINDOW     = 1,   /* 窗口限制扫描（预判区域） */
    SCAN_MODE_STATIC     = 2    /* 固定角度持续观测 */
} ScanMode_t;

/* 扫描方向 */
typedef enum {
    SCAN_DIR_INCREASING = 0,
    SCAN_DIR_DECREASING = 1
} ScanDir_t;

/* 扫描控制器 */
typedef struct {
    float current_angle;        /* 当前扫描角度 (°) */
    float step_deg;             /* 步进角 (°) */
    float min_angle;            /* 扫描范围下限 (°) */
    float max_angle;            /* 扫描范围上限 (°) */
    float window_center;        /* 窗口中心角（窗口模式） */
    float window_width;         /* 窗口宽度（窗口模式） */
    ScanMode_t mode;
    ScanDir_t  dir;
    uint8_t    active;          /* 是否激活 */
} ScanControl_t;

void  ScanControl_Init(ScanControl_t *ctrl, float step_deg, float min_angle, float max_angle);
void  ScanControl_SetMode(ScanControl_t *ctrl, ScanMode_t mode);
void  ScanControl_SetWindow(ScanControl_t *ctrl, float center, float width);
float ScanControl_GetNextAngle(ScanControl_t *ctrl);
void  ScanControl_Reset(ScanControl_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* __SCAN_CONTROL_H */