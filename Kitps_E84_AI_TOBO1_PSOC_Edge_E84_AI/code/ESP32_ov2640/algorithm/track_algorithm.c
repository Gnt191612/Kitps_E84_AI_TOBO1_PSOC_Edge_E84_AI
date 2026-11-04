/**
 * @file track_algorithm.c
 * @brief 跟踪算法封装实现：三高特征区域 SAD 模板匹配
 */

#include "track_algorithm.h"
#include "Vision/feature.h"
#include "sys/time.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* 像素到毫米的转换系数 (需根据实际标定)
 * 假设 160x120 @ 20cm 视场对应 ~300mm x 225mm
 * H7 端 config_mount.h 的 ESP32_PIXEL_TO_MM_X 必须与此一致 */
#define PIXEL_TO_MM_X   1.875f   /* 300mm / 160px */
#define PIXEL_TO_MM_Y   1.875f   /* 225mm / 120px */

/* ──── 初始化 ──── */
void TrackAlgo_Init(TrackAlgo_t *algo)
{
    if (!algo) return;
    memset(algo, 0, sizeof(TrackAlgo_t));

    MeanFilter_Init(&algo->mean_filter);
    KalmanFilter_Init(&algo->kalman_filter);
    Predict_Init(&algo->predictor);

    algo->roi_count = 0;
    algo->frame_count = 0;
    algo->use_kalman = 1;  /* 默认使用卡尔曼 */
    algo->seed_x = -1;
    algo->seed_y = -1;
}

void TrackAlgo_SetSeed(TrackAlgo_t *algo, int px, int py)
{
    if (!algo) return;
    algo->seed_x = px;
    algo->seed_y = py;
}

/* ──── 像素坐标转毫米 ──── */
static inline void pixel_to_mm(float px, float py, float *mm_x, float *mm_y)
{
    if (mm_x) *mm_x = px * PIXEL_TO_MM_X;
    if (mm_y) *mm_y = py * PIXEL_TO_MM_Y;
}

static inline int clampi(int v, int lo, int hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static void copy_patch(const uint8_t *img, int w, int x, int y, int size, uint8_t *dst)
{
    for (int j = 0; j < size; j++) {
        memcpy(&dst[j * size], &img[(y + j) * w + x], size);
    }
}

/* 模板在 (x,y) 处的平均绝对差；超过 limit_sum 时提前退出 */
static uint32_t patch_sad(const uint8_t *img, int w, int x, int y,
                          const uint8_t *tpl, int size, uint32_t limit_sum)
{
    uint32_t sad = 0;
    for (int j = 0; j < size; j++) {
        const uint8_t *row = &img[(y + j) * w + x];
        const uint8_t *trow = &tpl[j * size];
        for (int i = 0; i < size; i++) {
            sad += (uint32_t)abs((int)row[i] - (int)trow[i]);
        }
        if (sad >= limit_sum) return sad;
    }
    return sad;
}

/* 在 (x0,y0) 周围 ±radius（步长 step）内找最佳匹配，返回平均绝对差 */
static float search_patch(const uint8_t *img, int w, int h,
                          const uint8_t *tpl, int size,
                          int x0, int y0, int radius, int step,
                          int *best_x, int *best_y)
{
    uint32_t best = UINT32_MAX;
    int bx = x0, by = y0;
    int xmin = clampi(x0 - radius, 0, w - size), xmax = clampi(x0 + radius, 0, w - size);
    int ymin = clampi(y0 - radius, 0, h - size), ymax = clampi(y0 + radius, 0, h - size);
    for (int y = ymin; y <= ymax; y += step) {
        for (int x = xmin; x <= xmax; x += step) {
            uint32_t s = patch_sad(img, w, x, y, tpl, size, best);
            if (s < best) {
                best = s;
                bx = x;
                by = y;
            }
        }
    }
    *best_x = bx;
    *best_y = by;
    return (best == UINT32_MAX) ? 1e9f : (float)best / (float)(size * size);
}

#if !EXP_TRACK_FULL_FRAME
static int cmp_int(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}
#endif

/* ──── 建立模型：在种子附近（或全图）选三个高特征区域并保存模板 ──── */
static int build_model(TrackAlgo_t *algo, const uint8_t *frame, int w, int h)
{
#if EXP_TRACK_FULL_FRAME
    int cx = (algo->seed_x >= 0) ? algo->seed_x : w / 2;
    int cy = (algo->seed_y >= 0) ? algo->seed_y : h / 2;
    algo->full_x = clampi(cx - TRACK_FULL_TPL_SIZE / 2, 0, w - TRACK_FULL_TPL_SIZE);
    algo->full_y = clampi(cy - TRACK_FULL_TPL_SIZE / 2, 0, h - TRACK_FULL_TPL_SIZE);
    copy_patch(frame, w, algo->full_x, algo->full_y, TRACK_FULL_TPL_SIZE, algo->full_tpl);
    algo->cx = algo->full_x + TRACK_FULL_TPL_SIZE / 2.0f;
    algo->cy = algo->full_y + TRACK_FULL_TPL_SIZE / 2.0f;
    algo->has_model = 1;
    return 0;
#else
    int rx = 0, ry = 0, rw = w, rh = h;
    if (algo->seed_x >= 0 && algo->seed_y >= 0) {
        rx = clampi(algo->seed_x - TRACK_SEED_WIN_W / 2, 0, w - TRACK_SEED_WIN_W);
        ry = clampi(algo->seed_y - TRACK_SEED_WIN_H / 2, 0, h - TRACK_SEED_WIN_H);
        rw = TRACK_SEED_WIN_W;
        rh = TRACK_SEED_WIN_H;
    }
    algo->roi_count = ROI_SelectInRect(frame, w, h, rx, ry, rw, rh,
                                       algo->rois, ROI_MAX_COUNT);
    if (algo->roi_count == 0) return -1;

    float sx = 0, sy = 0;
    for (int i = 0; i < algo->roi_count; i++) {
        copy_patch(frame, w, algo->rois[i].x, algo->rois[i].y,
                   TRACK_TPL_SIZE, algo->templates[i]);
        sx += algo->rois[i].x + TRACK_TPL_SIZE / 2.0f;
        sy += algo->rois[i].y + TRACK_TPL_SIZE / 2.0f;
    }
    /* 有种子时目标中心以种子为准（H7 已把云台对准目标），否则取区域中心均值 */
    if (algo->seed_x >= 0 && algo->seed_y >= 0) {
        algo->cx = (float)algo->seed_x;
        algo->cy = (float)algo->seed_y;
    } else {
        algo->cx = sx / algo->roi_count;
        algo->cy = sy / algo->roi_count;
    }
    algo->has_model = 1;
    return 0;
#endif
}

/* ──── 匹配一帧：成功返回 0 并更新 cx/cy ──── */
static int match_frame(TrackAlgo_t *algo, const uint8_t *frame, int w, int h,
                       int radius)
{
#if EXP_TRACK_FULL_FRAME
    (void)radius;
    /* 对照组：全图粗搜（步长 2）+ 局部精搜 */
    int bx, by;
    search_patch(frame, w, h, algo->full_tpl, TRACK_FULL_TPL_SIZE,
                 w / 2, h / 2, (w > h ? w : h), 2, &bx, &by);
    float mad = search_patch(frame, w, h, algo->full_tpl, TRACK_FULL_TPL_SIZE,
                             bx, by, 2, 1, &bx, &by);
    if (mad > TRACK_MAD_MAX) return -1;
    algo->full_x = bx;
    algo->full_y = by;
    copy_patch(frame, w, bx, by, TRACK_FULL_TPL_SIZE, algo->full_tpl);
    algo->cx = bx + TRACK_FULL_TPL_SIZE / 2.0f;
    algo->cy = by + TRACK_FULL_TPL_SIZE / 2.0f;
    return 0;
#else
    int nx[ROI_MAX_COUNT], ny[ROI_MAX_COUNT], dx[ROI_MAX_COUNT], dy[ROI_MAX_COUNT];
    int ok[ROI_MAX_COUNT];
    int n_ok = 0;

    for (int i = 0; i < algo->roi_count; i++) {
        float mad;
        if (radius > 2 * TRACK_SEARCH_PX) {
            /* 复锁的大窗口：步长 2 粗搜后 ±1 精搜，控制耗时 */
            int cx0, cy0;
            search_patch(frame, w, h, algo->templates[i], TRACK_TPL_SIZE,
                         algo->rois[i].x, algo->rois[i].y, radius, 2, &cx0, &cy0);
            mad = search_patch(frame, w, h, algo->templates[i], TRACK_TPL_SIZE,
                               cx0, cy0, 1, 1, &nx[i], &ny[i]);
        } else {
            mad = search_patch(frame, w, h, algo->templates[i], TRACK_TPL_SIZE,
                               algo->rois[i].x, algo->rois[i].y, radius, 1,
                               &nx[i], &ny[i]);
        }
        ok[i] = (mad <= TRACK_MAD_MAX);
        dx[i] = nx[i] - algo->rois[i].x;
        dy[i] = ny[i] - algo->rois[i].y;
        if (ok[i]) n_ok++;
    }
    if (n_ok == 0) return -1;

    /* 离群剔除：各区域应随目标一起平移 */
    int mdx[ROI_MAX_COUNT], mdy[ROI_MAX_COUNT], m = 0;
    for (int i = 0; i < algo->roi_count; i++) {
        if (ok[i]) { mdx[m] = dx[i]; mdy[m] = dy[i]; m++; }
    }
    qsort(mdx, m, sizeof(int), cmp_int);
    qsort(mdy, m, sizeof(int), cmp_int);
    int med_dx = mdx[m / 2], med_dy = mdy[m / 2];

    float shift_x = 0, shift_y = 0;
    int kept = 0;
    for (int i = 0; i < algo->roi_count; i++) {
        if (!ok[i]) continue;
        if (abs(dx[i] - med_dx) > TRACK_OUTLIER_PX ||
            abs(dy[i] - med_dy) > TRACK_OUTLIER_PX) continue;
        shift_x += dx[i];
        shift_y += dy[i];
        kept++;
    }
    if (kept == 0) return -1;
    shift_x /= kept;
    shift_y /= kept;

    /* 所有区域整体平移（失配的区域也跟着走，下一帧还能匹配回来）；
     * 匹配良好的区域模板缓慢更新以适应外观变化 */
    for (int i = 0; i < algo->roi_count; i++) {
        int good = ok[i] && abs(dx[i] - med_dx) <= TRACK_OUTLIER_PX &&
                   abs(dy[i] - med_dy) <= TRACK_OUTLIER_PX;
        if (good) {
            algo->rois[i].x = nx[i];
            algo->rois[i].y = ny[i];
            uint8_t *tpl = algo->templates[i];
            for (int j = 0; j < TRACK_TPL_SIZE; j++) {
                const uint8_t *row = &frame[(ny[i] + j) * w + nx[i]];
                for (int k = 0; k < TRACK_TPL_SIZE; k++) {
                    uint8_t *t = &tpl[j * TRACK_TPL_SIZE + k];
                    *t = (uint8_t)((3 * (int)*t + (int)row[k]) / 4);
                }
            }
        } else {
            algo->rois[i].x = clampi(algo->rois[i].x + (int)lroundf(shift_x), 0, w - TRACK_TPL_SIZE);
            algo->rois[i].y = clampi(algo->rois[i].y + (int)lroundf(shift_y), 0, h - TRACK_TPL_SIZE);
        }
    }
    algo->cx += shift_x;
    algo->cy += shift_y;
    if (algo->cx < 0) algo->cx = 0;
    if (algo->cy < 0) algo->cy = 0;
    if (algo->cx > w - 1) algo->cx = (float)(w - 1);
    if (algo->cy > h - 1) algo->cy = (float)(h - 1);
    return 0;
#endif
}

/* 毫秒时间戳；电脑端测试可用 -DTRACK_TIME_MS=xxx 换成模拟时钟 */
#ifndef TRACK_TIME_MS
static uint32_t track_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}
#define TRACK_TIME_MS() track_time_ms()
#else
uint32_t TRACK_TIME_MS(void);
#endif

static void filter_output(TrackAlgo_t *algo, float *result_x, float *result_y)
{
    float mm_x, mm_y;
    pixel_to_mm(algo->cx, algo->cy, &mm_x, &mm_y);

    if (algo->use_kalman) {
        uint32_t now_ms = TRACK_TIME_MS();
        /* 每个跟踪器自己记上一帧时刻（原先是函数内 static，各次跟踪之间互相影响） */
        float dt = (algo->prev_time_ms > 0) ? (now_ms - algo->prev_time_ms) / 1000.0f : 0.033f;
        if (dt <= 0.0f || dt > 0.5f) dt = 0.033f;  /* 限制 dt */
        algo->prev_time_ms = now_ms;

        KalmanFilter_Update(&algo->kalman_filter, mm_x, mm_y, dt, result_x, result_y);

        float pred_x, pred_y;
        Predict_Update(&algo->predictor, *result_x, *result_y, dt, &pred_x, &pred_y);
        /* 这里保留滤波结果, 预测结果用于闭环控制 */
    } else {
        MeanFilter_Update(&algo->mean_filter, mm_x, mm_y, result_x, result_y);
    }
}

/* ──── 处理一帧 ──── */
int TrackAlgo_Process(TrackAlgo_t *algo, const uint8_t *frame, int w, int h,
                      float *result_x, float *result_y)
{
    if (!algo || !frame) return -1;

    algo->frame_count++;

    if (!algo->has_model) {
        if (build_model(algo, frame, w, h) != 0) return -1;
        pixel_to_mm(algo->cx, algo->cy, result_x, result_y);
        return 0;
    }

    if (match_frame(algo, frame, w, h, TRACK_SEARCH_PX) != 0) return -1;
    filter_output(algo, result_x, result_y);
    return 0;
}

int TrackAlgo_Reacquire(TrackAlgo_t *algo, const uint8_t *frame, int w, int h,
                        int cx, int cy, int radius,
                        float *result_x, float *result_y)
{
    if (!algo || !frame || !algo->has_model) return -1;

#if EXP_TRACK_FULL_FRAME
    (void)cx; (void)cy; (void)radius;   /* 全图模式每帧本就搜全图 */
#else
    if (cx >= 0 && cy >= 0) {
        /* 把区域整体搬到新的搜索中心，保持相对布局 */
        int sx = cx - (int)lroundf(algo->cx), sy = cy - (int)lroundf(algo->cy);
        for (int i = 0; i < algo->roi_count; i++) {
            algo->rois[i].x = clampi(algo->rois[i].x + sx, 0, w - TRACK_TPL_SIZE);
            algo->rois[i].y = clampi(algo->rois[i].y + sy, 0, h - TRACK_TPL_SIZE);
        }
        algo->cx = (float)cx;
        algo->cy = (float)cy;
    }
#endif
    if (match_frame(algo, frame, w, h, radius) != 0) return -1;
    pixel_to_mm(algo->cx, algo->cy, result_x, result_y);
    return 0;
}

/* ──── 重置 ──── */
void TrackAlgo_Reset(TrackAlgo_t *algo)
{
    TrackAlgo_Init(algo);
}
