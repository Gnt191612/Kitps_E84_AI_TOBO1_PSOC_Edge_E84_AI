/**
 * @file    radar_filter.h
 * @brief   多级点云去噪与平滑滤波
 */

#ifndef __RADAR_FILTER_H
#define __RADAR_FILTER_H

#include "radar_process.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 宏定义
 *----------------------------------------------------------------------------*/
#define FILTER_WINDOW_SIZE      5       /* 滑动平均窗口帧数 */
#define OUTLIER_THRESH_CM       30.0f   /* 限幅滤波：单帧突变超过30cm视为噪声 */

/*----------------------------------------------------------------------------
 * API 函数声明
 *----------------------------------------------------------------------------*/
void     Radar_Filter_Init(void);
uint8_t  Radar_Filter_Apply(RawPoint_t *points, uint8_t count);
uint8_t  Radar_Filter_GetCleaned(RawPoint_t *out, uint8_t max_num);

#ifdef __cplusplus
}
#endif

#endif /* __RADAR_FILTER_H */