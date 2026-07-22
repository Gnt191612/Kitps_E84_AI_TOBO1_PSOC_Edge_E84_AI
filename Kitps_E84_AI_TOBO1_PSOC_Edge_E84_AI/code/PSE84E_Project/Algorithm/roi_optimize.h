/**
 * @file    roi_optimize.h
 * @brief   扫描数据分区块不完全计算与高特征区域细扫
 * 
 * 公开接口：
 *   ROI_Optimize_Init()        - 初始化
 *   ROI_Optimize_CoarseScan()  - 粗扫描（低分辨率能量计算），返回候选高特征块列表
 *   ROI_Optimize_FineScan()    - 对指定块进行精细扫描，产生NPU推理输入数据
 *   ROI_Optimize_GetCandidates() - 获取上次粗扫选出的高特征区块编号
 * 
 * 说明：
 *   粗扫：将全角度范围等分成 N 个扇区，每个扇区仅采集少量点（或单点）强度值，
 *         计算能量和，选择前 K 个能量最高的扇区作为细扫候选。
 *   细扫：对每个候选扇区以更小的角度步进采集完整回波数据，整理成 NPU 输入格式。
 */

#ifndef __ROI_OPTIMIZE_H
#define __ROI_OPTIMIZE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 粗扫分区数量 */
#define ROI_SECTOR_COUNT        12
/* 最多高特征候选区数量 */
#define ROI_MAX_CANDIDATES      3
/* 细扫角度步进（度） */
#define ROI_FINE_STEP_DEG       1.0f

/* 粗扫返回的扇区能量信息 */
typedef struct {
    uint8_t sector_id;
    float   energy;
} SectorEnergy_t;

/* 细扫数据缓冲区：每个扇区最多采样点数 */
#define ROI_FINE_SAMPLES_PER_SECTOR  20

/* 指向具体硬件：设置雷达扫描角度并获取回波强度（由外部实现） */
extern void Radar_SetAngle(float angle_deg);
extern float Radar_GetEchoStrength(void);

void  ROI_Optimize_Init(float angle_min, float angle_max);
uint8_t ROI_Optimize_CoarseScan(SectorEnergy_t *sectors, uint8_t max_sectors);
uint8_t ROI_Optimize_GetCandidates(SectorEnergy_t *sectors, uint8_t total_count,
                                   uint8_t *candidate_ids, uint8_t max_candidates);
uint8_t ROI_Optimize_FineScan(uint8_t sector_id, float *data_buffer, uint8_t max_samples);

#ifdef __cplusplus
}
#endif

#endif /* __ROI_OPTIMIZE_H */