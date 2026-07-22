/**
 * @file    radar_filter.c
 * @brief   多级点云去噪实现：限幅滤波 + 中值滤波 + 滑动平均
 * 
 * 噪声来源：
 *   - 脉冲式毛刺（由电磁干扰或电源纹波引起）
 *   - 距离值抖动（由声波反射角度、多径效应引起）
 *   - 偶尔出现的异常大跳变（由二次回波或串扰引起）
 * 
 * 三级滤波策略：
 *   第一级 限幅：与前帧参考值突变 >30cm → 标记无效
 *   第二级 中值：当前帧内所有点的距离取中值，剔除偏离>30%的点
 *   第三级 滑动平均：连续5帧同一点的距离取均值，平滑小幅度抖动
 */

#include "radar_filter.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/*----------------------------------------------------------------------------
 * 内部缓存
 *----------------------------------------------------------------------------*/
static RawPoint_t s_hist[FILTER_WINDOW_SIZE][RAW_POINTS_MAX];
static uint8_t    s_hist_cnt[FILTER_WINDOW_SIZE];
static uint8_t    s_hist_pos = 0;

static RawPoint_t s_cleaned[RAW_POINTS_MAX];
static uint8_t    s_cleaned_cnt = 0;

/* qsort辅助比较函数 */
static int cmp_float(const void *a, const void *b)
{
    float diff = *(float*)a - *(float*)b;
    return (diff > 0.0f) ? 1 : ((diff < 0.0f) ? -1 : 0);
}

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void Radar_Filter_Init(void)
{
    memset(s_hist,      0, sizeof(s_hist));
    memset(s_hist_cnt,  0, sizeof(s_hist_cnt));
    memset(s_cleaned,   0, sizeof(s_cleaned));
    s_hist_pos    = 0;
    s_cleaned_cnt = 0;
}


/*----------------------------------------------------------------------------
 * 多级滤波核心
 *----------------------------------------------------------------------------*/
uint8_t Radar_Filter_Apply(RawPoint_t *points, uint8_t count)
{
    if (count == 0 || !points) return 0;

    /* -------- 第一级：限幅滤波 -------- */
    for (int i = count - 1; i >= 0; i--) {
        uint8_t prev_idx = (s_hist_pos == 0) ? (FILTER_WINDOW_SIZE - 1) : (s_hist_pos - 1);
        if (s_hist_cnt[prev_idx] > 0) {
            float prev = s_hist[prev_idx][0].distance_cm;
            if (fabsf(points[i].distance_cm - prev) > OUTLIER_THRESH_CM) {
                /* 移除跳变点 */
                memmove(&points[i], &points[i + 1], (count - i - 1) * sizeof(RawPoint_t));
                count--;
            }
        }
    }
    if (count == 0) return 0;

    /* -------- 第二级：中值滤波 -------- */
    if (count > 1) {
        float tmp[RAW_POINTS_MAX];  /* 栈分配（RAW_POINTS_MAX=16，仅 64 字节），避免 malloc */
        for (uint8_t i = 0; i < count && i < RAW_POINTS_MAX; i++) tmp[i] = points[i].distance_cm;
        qsort(tmp, count, sizeof(float), cmp_float);
        float median = tmp[count / 2];

        for (int i = count - 1; i >= 0; i--) {
            if (fabsf(points[i].distance_cm - median) > median * 0.3f) {
                memmove(&points[i], &points[i + 1], (count - i - 1) * sizeof(RawPoint_t));
                count--;
            }
        }
    }
    if (count == 0) return 0;

    /* -------- 第三级：滑动平均 -------- */
    memcpy(s_hist[s_hist_pos], points, count * sizeof(RawPoint_t));
    s_hist_cnt[s_hist_pos] = count;
    s_hist_pos = (s_hist_pos + 1) % FILTER_WINDOW_SIZE;

    s_cleaned_cnt = 0;
    for (uint8_t p = 0; p < count && s_cleaned_cnt < RAW_POINTS_MAX; p++) {
        float   sum       = 0.0f;
        uint8_t valid_cnt = 0;

        for (uint8_t w = 0; w < FILTER_WINDOW_SIZE; w++) {
            if (s_hist_cnt[w] > p) {
                sum += s_hist[w][p].distance_cm;
                valid_cnt++;
            }
        }

        if (valid_cnt > 0) {
            s_cleaned[s_cleaned_cnt]               = points[p];
            s_cleaned[s_cleaned_cnt].distance_cm   = sum / valid_cnt;
            s_cleaned_cnt++;
        }
    }

    return s_cleaned_cnt;
}


/*----------------------------------------------------------------------------
 * 获取去噪后点云
 *----------------------------------------------------------------------------*/
uint8_t Radar_Filter_GetCleaned(RawPoint_t *out, uint8_t max_num)
{
    uint8_t n = (s_cleaned_cnt < max_num) ? s_cleaned_cnt : max_num;
    memcpy(out, s_cleaned, n * sizeof(RawPoint_t));
    return n;
}