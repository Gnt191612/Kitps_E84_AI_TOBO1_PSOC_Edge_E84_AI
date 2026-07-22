/**
 * @file    scan_control.c
 * @brief   扫描控制实现
 * 
 * 功能：
 *   - 全范围往复：在[min, max]之间以设定步进步进，到达边界反向
 *   - 窗口模式：在[center-width/2, center+width/2]内往复扫描
 *   - 静态模式：返回固定中心角度
 */

#include "scan_control.h"
#include <math.h>

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void ScanControl_Init(ScanControl_t *ctrl, float step_deg, float min_angle, float max_angle)
{
    ctrl->current_angle = min_angle;
    ctrl->step_deg      = step_deg;
    ctrl->min_angle     = min_angle;
    ctrl->max_angle     = max_angle;
    ctrl->window_center = 0.0f;
    ctrl->window_width  = max_angle - min_angle;
    ctrl->mode          = SCAN_MODE_FULL_RANGE;
    ctrl->dir           = SCAN_DIR_INCREASING;
    ctrl->active        = 1;
}

/*----------------------------------------------------------------------------
 * 设置扫描模式
 *----------------------------------------------------------------------------*/
void ScanControl_SetMode(ScanControl_t *ctrl, ScanMode_t mode)
{
    ctrl->mode = mode;
    ScanControl_Reset(ctrl);   /* 切换模式时复位方向 */
}

/*----------------------------------------------------------------------------
 * 设置窗口参数
 *----------------------------------------------------------------------------*/
void ScanControl_SetWindow(ScanControl_t *ctrl, float center, float width)
{
    ctrl->window_center = center;
    ctrl->window_width  = width;
}

/*----------------------------------------------------------------------------
 * 获取下一个扫描角度
 *----------------------------------------------------------------------------*/
float ScanControl_GetNextAngle(ScanControl_t *ctrl)
{
    if (!ctrl->active) return 0.0f;

    float low, high;
    if (ctrl->mode == SCAN_MODE_WINDOW) {
        float half = ctrl->window_width / 2.0f;
        low  = ctrl->window_center - half;
        high = ctrl->window_center + half;
        /* 边界约束在全局范围内 */
        if (low  < ctrl->min_angle) low  = ctrl->min_angle;
        if (high > ctrl->max_angle) high = ctrl->max_angle;
    } else {
        low  = ctrl->min_angle;
        high = ctrl->max_angle;
    }

    if (ctrl->mode == SCAN_MODE_STATIC) {
        return ctrl->window_center;   /* 固定角度 */
    }

    /* 往复扫描逻辑 */
    float next_angle = ctrl->current_angle;

    if (ctrl->dir == SCAN_DIR_INCREASING) {
        next_angle += ctrl->step_deg;
        if (next_angle > high) {
            next_angle = high;
            ctrl->dir = SCAN_DIR_DECREASING;
        }
    } else {
        next_angle -= ctrl->step_deg;
        if (next_angle < low) {
            next_angle = low;
            ctrl->dir = SCAN_DIR_INCREASING;
        }
    }

    ctrl->current_angle = next_angle;
    return next_angle;
}

void ScanControl_Reset(ScanControl_t *ctrl)
{
    ctrl->dir = SCAN_DIR_INCREASING;
    if (ctrl->mode == SCAN_MODE_WINDOW) {
        float half = ctrl->window_width / 2.0f;
        ctrl->current_angle = ctrl->window_center - half;
        if (ctrl->current_angle < ctrl->min_angle)
            ctrl->current_angle = ctrl->min_angle;
    } else {
        ctrl->current_angle = ctrl->min_angle;
    }
}