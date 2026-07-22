/**
 * @file    target_detect.c
 * @brief   目标检测实现：DBSCAN聚类 + 方位几何推断 + 坐标转换
 * 
 * 核心流程：
 *  1. DBSCAN聚类：将距离差 ≤ CLUSTER_EPS_CM 的点归为同一目标
 *  2. 方位推断：根据传感器安装基线(baseline)和目标距离，用三角定位反算角度
 *  3. 坐标变换：将(距离, 角度)转换为笛卡尔(x, y)坐标，供卡尔曼滤波器使用
 * 
 * 方位推断原理(双传感器三角定位法):
 *       sensor0 —— baseline —— sensor1
 *          \                    /
 *       d0  \  目标(x,y)      /  d1
 *            \               /
 *             └─────────────┘
 *   已知基线长度B和两个距离d0、d1，根据余弦定理：
 *     angle = acos((d0² + B² - d1²) / (2 × d0 × B))
 *   最后将(距离, 角度)转换为笛卡尔坐标:
 *     x = d × cos(angle)
 *     y = d × sin(angle)
 */

#include "target_detect.h"
#include <string.h>

/*----------------------------------------------------------------------------
 * 内部宏
 *----------------------------------------------------------------------------*/
#define UNVISITED   ((int8_t)(-1))
#define NOISE       ((int8_t)(-2))
#define CLUSTER_MAX 10

/*----------------------------------------------------------------------------
 * 全局状态
 *----------------------------------------------------------------------------*/
static int8_t s_labels[RAW_POINTS_MAX];
static int8_t s_neighbor_buffer[RAW_POINTS_MAX];

/*----------------------------------------------------------------------------
 * 辅助函数：两点距离差（绝对值，单位cm）
 *----------------------------------------------------------------------------*/
static float point_dist(RawPoint_t *a, RawPoint_t *b)
{
    return fabsf(a->distance_cm - b->distance_cm);
}

/*----------------------------------------------------------------------------
 * 区域查询
 *----------------------------------------------------------------------------*/
static uint8_t region_query(RawPoint_t *pts, uint8_t cnt, uint8_t idx, float eps)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < cnt; i++) {
        if (i != idx && point_dist(&pts[idx], &pts[i]) <= eps) {
            s_neighbor_buffer[n++] = (int8_t)i;
        }
    }
    return n;
}

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void TargetDetect_Init(void)
{
    memset(s_labels, UNVISITED, sizeof(s_labels));
    memset(s_neighbor_buffer, 0, sizeof(s_neighbor_buffer));
}


/*----------------------------------------------------------------------------
 * DBSCAN 聚类核心 (含单点绕过逻辑)
 * 单传感器场景（HC-SR04 每帧仅1个点）时直接构造候选，跳过聚类。
 *----------------------------------------------------------------------------*/
uint8_t TargetDetect_Cluster(RawPoint_t *points, uint8_t cnt, TargetCandidate_t *targets)
{
    if (cnt == 0 || !points || !targets) return 0;

    /* ▼ 单点绕过：HC-SR04 每帧只产生1个点，DBSCAN 无法成簇 */
    if (cnt == 1) {
        targets[0].id          = 1;
        targets[0].x_cm       = 0.0f;  /* 后续由 TargetDetect_Coordinate_Transform 填充 */
        targets[0].y_cm       = 0.0f;
        targets[0].distance_cm = points[0].distance_cm;
        targets[0].angle_deg   = points[0].angle_deg;
        targets[0].confidence  = SINGLE_POINT_CONFIDENCE;
        targets[0].valid       = 1;
        return 1;
    }
    /* ▲ */

    /* 标记初始化 */
    for (uint8_t i = 0; i < cnt; i++) s_labels[i] = UNVISITED;

    uint8_t cluster_count = 0;

    for (uint8_t i = 0; i < cnt; i++) {
        if (s_labels[i] != UNVISITED) continue;

        uint8_t nb_cnt = region_query(points, cnt, i, CLUSTER_EPS_CM);

        if (nb_cnt < CLUSTER_MIN_POINTS) {
            s_labels[i] = NOISE;
        } else {
            uint8_t current_cluster = cluster_count++;

            /* 扩展聚类 */
            s_labels[i] = (int8_t)current_cluster;
            for (uint8_t j = 0; j < nb_cnt; j++) {
                uint8_t neighbor_idx = (uint8_t)s_neighbor_buffer[j];
                if (s_labels[neighbor_idx] == UNVISITED) {
                    s_labels[neighbor_idx] = (int8_t)current_cluster;
                }
            }
        }

        if (cluster_count >= CLUSTER_MAX) break;
    }

    /* 对每个聚类生成一个目标候选 */
    uint8_t target_count = 0;
    for (uint8_t c = 0; c < cluster_count && target_count < TARGET_MAX_PER_FRAME; c++) {
        float   sum_dist  = 0.0f;
        float   sum_angle = 0.0f;
        uint8_t valid_cnt = 0;

        for (uint8_t i = 0; i < cnt; i++) {
            if (s_labels[i] == (int8_t)c) {
                sum_dist  += points[i].distance_cm;
                sum_angle += points[i].angle_deg;
                valid_cnt++;
            }
        }

        if (valid_cnt > 0) {
            targets[target_count].id          = target_count + 1;
            targets[target_count].distance_cm = sum_dist / valid_cnt;
            targets[target_count].angle_deg   = sum_angle / valid_cnt;
            targets[target_count].confidence  = (float)valid_cnt / (float)cnt;
            targets[target_count].valid       = 1;
            target_count++;
        }
    }

    return target_count;
}


/*----------------------------------------------------------------------------
 * 坐标变换： (距离, 角度) → (x, y) 笛卡尔坐标
 *----------------------------------------------------------------------------*/
uint8_t TargetDetect_Coordinate_Transform(TargetCandidate_t *targets, uint8_t cnt)
{
    for (uint8_t i = 0; i < cnt; i++) {
        if (!targets[i].valid) continue;

        float rad = targets[i].angle_deg * (float)M_PI / 180.0f;
        targets[i].x_cm = targets[i].distance_cm * cosf(rad);
        targets[i].y_cm = targets[i].distance_cm * sinf(rad);
    }
    return cnt;
}