/**
 * @file    roi_optimize.c
 * @brief   分区块不完全计算与细扫实现
 * 
 * 算法流程：
 *   1. 粗扫：扫描全范围，每个扇区取中心角度采样一次回波强度，存入能量数组
 *   2. 排序选出能量最高的 K 个扇区
 *   3. 细扫：对选出的扇区，从扇区边界到边界以 FINE_STEP 步进采集完整回波强度序列，
 *             作为 NPU 输入
 */

#include "roi_optimize.h"
#include <string.h>
#include <stdlib.h>

/* 内部全局变量 */
static float g_angle_min;
static float g_angle_max;
static float g_sector_width;

/* 比较函数（降序） */
static int compare_energy(const void *a, const void *b) {
    float ea = ((SectorEnergy_t*)a)->energy;
    float eb = ((SectorEnergy_t*)b)->energy;
    return (ea < eb) ? 1 : ((ea > eb) ? -1 : 0);
}

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void ROI_Optimize_Init(float angle_min, float angle_max)
{
    g_angle_min = angle_min;
    g_angle_max = angle_max;
    g_sector_width = (angle_max - angle_min) / ROI_SECTOR_COUNT;
}

/*----------------------------------------------------------------------------
 * 粗扫：遍历所有扇区，每个扇区中心角采集一次回波强度
 *----------------------------------------------------------------------------*/
uint8_t ROI_Optimize_CoarseScan(SectorEnergy_t *sectors, uint8_t max_sectors)
{
    if (!sectors || max_sectors < ROI_SECTOR_COUNT) return 0;

    for (uint8_t i = 0; i < ROI_SECTOR_COUNT; i++) {
        float center_angle = g_angle_min + (i + 0.5f) * g_sector_width;
        Radar_SetAngle(center_angle);
        /* 等待机械稳定（若有） */
        float energy = Radar_GetEchoStrength();
        sectors[i].sector_id = i;
        sectors[i].energy = energy;
    }
    return ROI_SECTOR_COUNT;
}

/*----------------------------------------------------------------------------
 * 获取高能量候选扇区
 *----------------------------------------------------------------------------*/
uint8_t ROI_Optimize_GetCandidates(SectorEnergy_t *sectors, uint8_t total_count,
                                   uint8_t *candidate_ids, uint8_t max_candidates)
{
    if (!sectors || !candidate_ids || total_count == 0 || max_candidates == 0)
        return 0;

    /* 按能量排序 */
    qsort(sectors, total_count, sizeof(SectorEnergy_t), compare_energy);

    uint8_t num = (max_candidates < total_count) ? max_candidates : total_count;
    for (uint8_t i = 0; i < num; i++) {
        candidate_ids[i] = sectors[i].sector_id;
    }
    return num;
}

/*----------------------------------------------------------------------------
 * 细扫：对一个扇区内部精细步进扫描，生成输入数据
 *----------------------------------------------------------------------------*/
uint8_t ROI_Optimize_FineScan(uint8_t sector_id, float *data_buffer, uint8_t max_samples)
{
    if (sector_id >= ROI_SECTOR_COUNT || !data_buffer) return 0;

    float start_angle = g_angle_min + sector_id * g_sector_width;
    float end_angle   = start_angle + g_sector_width;
    uint8_t count = 0;

    for (float angle = start_angle; angle <= end_angle; angle += ROI_FINE_STEP_DEG) {
        if (count >= max_samples) break;
        Radar_SetAngle(angle);
        data_buffer[count++] = Radar_GetEchoStrength();
    }
    return count;
}