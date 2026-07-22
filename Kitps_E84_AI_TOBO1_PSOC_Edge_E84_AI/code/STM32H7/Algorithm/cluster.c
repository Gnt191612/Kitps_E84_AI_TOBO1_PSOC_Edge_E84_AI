/**
 * @file    cluster.c
 * @brief   DBSCAN 聚类算法实现
 *
 * 实现 DBSCAN (Density-Based Spatial Clustering of Applications with Noise)
 * 算法，将雷达点云中密集的点聚合成候选目标。
 *
 * 步骤：
 *   1. 对每个有效点做极坐标→直角坐标转换
 *   2. 遍历每个点，收集邻域内点
 *   3. 邻域点数 >= CLUSTER_MIN_POINTS 的为核心点，展开聚类
 *   4. 每个簇计算质心作为目标候选
 *   5. 质心反算极坐标填入 TargetCandidate_t
 */

#include "cluster.h"
#include <math.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * 常量定义
 *----------------------------------------------------------------------------*/
#define DEG_TO_RAD_FACTOR   (3.14159265358979f / 180.0f)
#define CLUSTER_MAX_POINTS  16          /* 最大输入点数 */
#define CLUSTER_MAX_CLUSTERS 3          /* 最多输出目标数 */

/* 内部点标签 */
#define LABEL_UNDEFINED   (-1)
#define LABEL_NOISE       (0)

/*----------------------------------------------------------------------------
 * 内部数据结构
 *----------------------------------------------------------------------------*/
typedef struct {
    float x;        /* X坐标(cm) */
    float y;        /* Y坐标(cm) */
    float dist_cm;  /* 径向距离(cm) */
    float angle_deg;/* 方位角(°) */
    int   cluster;  /* 所属簇编号, -1=未分类, 0=噪声, >0=簇ID */
    int   visited;  /* 0=未访问, 1=已访问 */
} Point2D_t;

/*----------------------------------------------------------------------------
 * 静态变量
 *----------------------------------------------------------------------------*/
static uint8_t g_initialized = 0;

/*----------------------------------------------------------------------------
 * 角度转弧度
 *----------------------------------------------------------------------------*/
static inline float DegToRad(float deg)
{
    return deg * DEG_TO_RAD_FACTOR;
}

/*----------------------------------------------------------------------------
 * 极坐标→直角坐标
 *----------------------------------------------------------------------------*/
static void PolarToCartesian(float dist_cm, float angle_deg,
                             float *x, float *y)
{
    float rad = DegToRad(angle_deg);
    *x = dist_cm * cosf(rad);
    *y = dist_cm * sinf(rad);
}

/*----------------------------------------------------------------------------
 * 直角坐标→极坐标
 *----------------------------------------------------------------------------*/
static void CartesianToPolar(float x, float y,
                             float *dist_cm, float *angle_deg)
{
    *dist_cm  = sqrtf(x * x + y * y);
    *angle_deg = atan2f(y, x) / DEG_TO_RAD_FACTOR;

    /* 归一化到 [0, 360) */
    if (*angle_deg < 0.0f) {
        *angle_deg += 360.0f;
    }
}

/*----------------------------------------------------------------------------
 * 计算两点欧氏距离（平面直角坐标）
 *----------------------------------------------------------------------------*/
static inline float PointDistance(const Point2D_t *a, const Point2D_t *b)
{
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return sqrtf(dx * dx + dy * dy);
}

/*----------------------------------------------------------------------------
 * 查找点 p_idx 在 eps 半径内的所有邻域点
 *----------------------------------------------------------------------------*/
static uint8_t FindNeighbors(const Point2D_t *points, uint8_t cnt,
                             uint8_t p_idx, float eps,
                             uint8_t *neighbors, uint8_t max_neighbors)
{
    uint8_t n_cnt = 0;
    for (uint8_t i = 0; i < cnt; i++) {
        if (!points[i].visited) continue;   /* 只考虑有效点 */
        if (i == p_idx) continue;           /* 不含自身（由调用者决定） */

        if (PointDistance(&points[p_idx], &points[i]) <= eps) {
            if (n_cnt < max_neighbors) {
                neighbors[n_cnt++] = i;
            }
        }
    }
    return n_cnt;
}

/*----------------------------------------------------------------------------
 * 全局邻居数（含自身）
 *----------------------------------------------------------------------------*/
static uint8_t CountNeighborsSelf(const Point2D_t *points, uint8_t cnt,
                                  uint8_t p_idx, float eps)
{
    uint8_t n = 1;  /* 含自身 */
    for (uint8_t i = 0; i < cnt; i++) {
        if (!points[i].visited) continue;
        if (i == p_idx) continue;
        if (PointDistance(&points[p_idx], &points[i]) <= eps) {
            n++;
        }
    }
    return n;
}

/*----------------------------------------------------------------------------
 * 扩展簇：将点 p_idx 所在簇扩展到所有密度可达的点
 *----------------------------------------------------------------------------*/
static void ExpandCluster(Point2D_t *points, uint8_t cnt,
                          uint8_t p_idx, uint8_t *neighbors,
                          uint8_t n_cnt,
                          float eps, uint8_t minPts,
                          int cluster_id)
{
    /* 标记当前点 */
    points[p_idx].cluster = cluster_id;

    /* 广度优先：遍历邻域点 */
    uint8_t seed_idx = 0;
    while (seed_idx < n_cnt) {
        uint8_t q_idx = neighbors[seed_idx];

        if (!points[q_idx].visited) {
            /* 理论上都已标记为 visited，忽略未访问 */
            points[q_idx].visited = 1;
        }

        if (points[q_idx].cluster == LABEL_UNDEFINED) {
            points[q_idx].cluster = cluster_id;

            /* 如果 q 也是核心点，加入其邻域继续扩展 */
            uint8_t q_cnt = CountNeighborsSelf(points, cnt, q_idx, eps);
            if (q_cnt >= minPts) {
                uint8_t q_neighbors[CLUSTER_MAX_POINTS];
                uint8_t q_ncnt = FindNeighbors(points, cnt, q_idx,
                                                eps, q_neighbors,
                                                CLUSTER_MAX_POINTS);
                for (uint8_t i = 0; i < q_ncnt; i++) {
                    uint8_t r_idx = q_neighbors[i];
                    if (points[r_idx].cluster == LABEL_UNDEFINED ||
                        points[r_idx].cluster == LABEL_NOISE) {
                        /* 如果未在种子列表中，添加 */
                        uint8_t already = 0;
                        for (uint8_t j = 0; j < n_cnt; j++) {
                            if (neighbors[j] == r_idx) {
                                already = 1;
                                break;
                            }
                        }
                        if (!already && n_cnt < CLUSTER_MAX_POINTS) {
                            neighbors[n_cnt++] = r_idx;
                        }
                    }
                }
            }
        }

        seed_idx++;
    }
}

/*----------------------------------------------------------------------------
 * 计算簇的质心
 *----------------------------------------------------------------------------*/
static void ComputeCentroid(const Point2D_t *points, uint8_t cnt,
                            int cluster_id,
                            float *cx, float *cy, uint8_t *members)
{
    float sx = 0.0f, sy = 0.0f;
    *members = 0;

    for (uint8_t i = 0; i < cnt; i++) {
        if (points[i].visited && points[i].cluster == cluster_id) {
            sx += points[i].x;
            sy += points[i].y;
            (*members)++;
        }
    }

    if (*members > 0) {
        *cx = sx / (float)(*members);
        *cy = sy / (float)(*members);
    } else {
        *cx = 0.0f;
        *cy = 0.0f;
    }
}

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void Cluster_Init(void)
{
    g_initialized = 1;
}

/*----------------------------------------------------------------------------
 * DBSCAN 聚类主函数
 *
 * 输入：
 *   points        - RawPoint_t 数组（原始雷达点云）
 *   cnt           - 输入点数量
 *   targets       - 输出目标候选数组（长度至少 TARGET_MAX_PER_FRAME）
 *
 * 返回：
 *   检测到的有效目标数（<= TARGET_MAX_PER_FRAME）
 *----------------------------------------------------------------------------*/
uint8_t Cluster_DBSCAN(RawPoint_t *points, uint8_t cnt,
                       TargetCandidate_t *targets)
{
    if (!g_initialized) {
        Cluster_Init();
    }

    if (points == NULL || targets == NULL || cnt == 0 || cnt > CLUSTER_MAX_POINTS) {
        return 0;
    }

    /*------------------------------------------------------------------------
     * Step 1: 极坐标→直角坐标，筛选有效点
     *------------------------------------------------------------------------*/
    Point2D_t pts[CLUSTER_MAX_POINTS];
    uint8_t   valid_cnt = 0;

    for (uint8_t i = 0; i < cnt && i < CLUSTER_MAX_POINTS; i++) {
        if (!points[i].valid) continue;

        PolarToCartesian(points[i].distance_cm, points[i].angle_deg,
                         &pts[valid_cnt].x, &pts[valid_cnt].y);
        pts[valid_cnt].dist_cm   = points[i].distance_cm;
        pts[valid_cnt].angle_deg = points[i].angle_deg;
        pts[valid_cnt].cluster   = LABEL_UNDEFINED;
        pts[valid_cnt].visited   = 1;   /* 有效点标记为可处理 */
        valid_cnt++;
    }

    if (valid_cnt == 0) return 0;

    /*------------------------------------------------------------------------
     * Step 2: DBSCAN 主循环
     *------------------------------------------------------------------------*/
    float eps    = CLUSTER_EPS_CM;
    uint8_t minPts = CLUSTER_MIN_POINTS;
    int cluster_id = 0;   /* 0 = noise, 1+ = clusters */

    /* 给每个点分配簇标签 */
    for (uint8_t i = 0; i < valid_cnt; i++) {
        if (pts[i].cluster != LABEL_UNDEFINED) {
            continue;   /* 已分类 */
        }

        uint8_t n_cnt = CountNeighborsSelf(pts, valid_cnt, i, eps);

        if (n_cnt < minPts) {
            pts[i].cluster = LABEL_NOISE;   /* 标记为噪声 */
            continue;
        }

        /* 核心点：创建新簇 */
        cluster_id++;

        uint8_t neighbors[CLUSTER_MAX_POINTS];
        uint8_t nb_cnt = FindNeighbors(pts, valid_cnt, i,
                                       eps, neighbors, CLUSTER_MAX_POINTS);
        ExpandCluster(pts, valid_cnt, i,
                      neighbors, nb_cnt,
                      eps, minPts, cluster_id);
    }

    /*------------------------------------------------------------------------
     * Step 3: 计算每个簇的质心，填入 TargetCandidate_t
     *------------------------------------------------------------------------*/
    uint8_t target_cnt = 0;

    for (int cid = 1; cid <= cluster_id && target_cnt < TARGET_MAX_PER_FRAME; cid++) {
        float cx, cy;
        uint8_t members;

        ComputeCentroid(pts, valid_cnt, cid, &cx, &cy, &members);

        if (members == 0) continue;

        float dist, angle_deg;
        CartesianToPolar(cx, cy, &dist, &angle_deg);

        float confidence = (float)members / (float)valid_cnt;
        if (confidence > 0.95f) confidence = 0.95f;
        if (confidence < 0.10f) confidence = 0.10f;

        targets[target_cnt].id         = target_cnt + 1;
        targets[target_cnt].x_cm       = cx;
        targets[target_cnt].y_cm       = cy;
        targets[target_cnt].distance_cm = dist;
        targets[target_cnt].angle_deg  = angle_deg;
        targets[target_cnt].confidence = confidence;
        targets[target_cnt].valid      = 1;

        target_cnt++;
    }

    /* 清空剩余 target 的 valid 标志 */
    for (uint8_t i = target_cnt; i < TARGET_MAX_PER_FRAME; i++) {
        targets[i].valid = 0;
    }

    return target_cnt;
}
