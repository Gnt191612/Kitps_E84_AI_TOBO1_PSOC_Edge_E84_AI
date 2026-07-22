/**
 * @file assist_pos.c
 * @brief 双 ESP32 互相辅助定位逻辑
 *
 * 接口: AssistPos_Init, AssistPos_Execute
 * 注: assist_pos.h 不存在, 本文件为 .c-only
 */

#include "data_logger/logger.h"
#include <string.h>
#include <math.h>

/* 辅助定位状态 */
typedef struct {
    float own_angle;         /* 自身角度 (°) */
    float other_angle;       /* 另一块板的定位角度 (°) */
    float combined_x;        /* 综合定位 X */
    float combined_y;        /* 综合定位 Y */
    int initialized;
} AssistPos_t;

static AssistPos_t s_assist;

/**
 * @brief 初始化辅助定位
 */
void AssistPos_Init(void)
{
    memset(&s_assist, 0, sizeof(AssistPos_t));
    s_assist.initialized = 1;
    LOG_INFO("AssistPos initialized");
}

/**
 * @brief 执行辅助定位计算 (三角定位)
 * @param own_angle   本板角度 (°)
 * @param other_angle 另一块板角度 (°)
 * @param base_dist   两块板之间的距离 (mm)
 * @param out_x       输出定位 X (mm)
 * @param out_y       输出定位 Y (mm)
 * @return 0=成功, -1=失败
 */
int AssistPos_Execute(float own_angle, float other_angle, float base_dist,
                      float *out_x, float *out_y)
{
    if (!s_assist.initialized) AssistPos_Init();
    if (!out_x || !out_y) return -1;

    s_assist.own_angle = own_angle;
    s_assist.other_angle = other_angle;

    /* 三角定位: 已知两个角度和基线长度, 计算目标位置 */
    /* 假设 ESP32-A 在 (0, 0), ESP32-B 在 (base_dist, 0) */
    /* 两直线交点即目标位置 */

    float alpha = own_angle * 3.14159265f / 180.0f;   /* 角度转弧度 */
    float beta  = other_angle * 3.14159265f / 180.0f;

    /* 目标角度相对于基线 */
    float tan_alpha = tanf(alpha);
    float tan_beta  = tanf(beta);

    /* 如果角度接近 0° 或 180°, 避免除零 */
    if (fabsf(alpha) < 0.01f || fabsf(beta) < 0.01f) {
        LOG_WARN("AssistPos: angle too small, using single-point estimate");
        s_assist.combined_x = base_dist / 2.0f;
        s_assist.combined_y = base_dist * tan_alpha;
        *out_x = s_assist.combined_x;
        *out_y = s_assist.combined_y;
        return 0;
    }

    /* 计算交点:
     * line1: y = tan(alpha) * x
     * line2: y = tan(beta) * (base_dist - x)
     * 交点: tan(alpha)*x = tan(beta)*(base_dist - x)
     *       x = base_dist * tan(beta) / (tan(alpha) + tan(beta))
     */
    float denom = tan_alpha + tan_beta;
    if (fabsf(denom) < 1e-6f) {
        LOG_WARN("AssistPos: parallel lines, cannot triangulate");
        return -1;
    }

    float x = base_dist * tan_beta / denom;
    float y = x * tan_alpha;

    /* 限幅防止异常值 */
    if (x < -1000 || x > 2000 || y < -1000 || y > 2000) {
        LOG_WARN("AssistPos: out of range (%.1f, %.1f), clamping", x, y);
        if (x < -1000) x = -1000;
        if (x > 2000) x = 2000;
        if (y < -1000) y = -1000;
        if (y > 2000) y = 2000;
    }

    s_assist.combined_x = x;
    s_assist.combined_y = y;

    *out_x = x;
    *out_y = y;

    LOG_INFO("AssistPos: own=%.1f°, other=%.1f°, base=%.0fmm → (%.1f, %.1f)mm",
             own_angle, other_angle, base_dist, x, y);
    return 0;
}

/**
 * @brief 获取最近一次的辅助定位结果
 */
void AssistPos_GetResult(float *out_x, float *out_y)
{
    if (out_x) *out_x = s_assist.combined_x;
    if (out_y) *out_y = s_assist.combined_y;
}
