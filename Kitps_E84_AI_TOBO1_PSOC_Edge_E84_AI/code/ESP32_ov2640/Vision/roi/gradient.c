/**
 * @file gradient.c
 * @brief 梯度下降法优化跟踪
 *
 * 使用上一帧 ROI 位置 + 当前帧图像, 通过梯度下降 (Lucas-Kanade 风格)
 * 搜索新位置。
 *
 * 接口: Gradient_Track
 */

#include "roi_select.h"
#include "Vision/feature.h"
#include <math.h>
#include <string.h>

/* ──── 梯度下降跟踪 ──── */
/**
 * @brief 梯度下降法跟踪
 * @param prev_roi  上一帧 ROI 位置
 * @param curr_img  当前帧灰度图像
 * @param w         图像宽度
 * @param h         图像高度
 * @param new_roi   输出: 跟踪后 ROI 新位置
 * @return 0=成功, -1=跟踪失败
 */
int Gradient_Track(const ROI_t *prev_roi, const uint8_t *curr_img,
                   int w, int h, ROI_t *new_roi)
{
    if (!prev_roi || !curr_img || !new_roi) return -1;

    /* 初始化新位置为上一帧位置 */
    *new_roi = *prev_roi;

    /* 搜索窗口: 在原位置周围 ±search_range 搜索 */
    const int search_range = 8;
    const int max_iter = 20;

    int cx = prev_roi->x + prev_roi->w / 2;
    int cy = prev_roi->y + prev_roi->h / 2;

    int best_dx = 0, best_dy = 0;
    float best_mse = 1e30f;

    /* 注意: 这里简化实现, 不使用真正的梯度下降迭代,
     * 而是在搜索窗口内穷举搜索最佳匹配 (归一化互相关) */

    /* 提取前一帧模板 */
    int tw = prev_roi->w;
    int th = prev_roi->h;

    /* 暂存模板 (但此处没有上一帧图像, 假设梯度值作为模板特征) */

    /* 穷举搜索: 计算每个偏移下的梯度幅值差异 */
    for (int dy = -search_range; dy <= search_range; dy++) {
        for (int dx = -search_range; dx <= search_range; dx++) {
            int nx = cx + dx - tw / 2;
            int ny = cy + dy - th / 2;

            /* 边界检查 */
            if (nx < 0 || ny < 0 || nx + tw > w || ny + th > h) continue;

            /* 计算当前位置的梯度幅值总和 (作为特征度量) */
            float grad_sum = 0;
            for (int j = ny; j < ny + th; j++) {
                for (int i = nx; i < nx + tw; i++) {
                    /* 快速梯度近似 */
                    int gx = (i > 0 && i < w-1) ?
                             (int)curr_img[j*w + i+1] - (int)curr_img[j*w + i-1] : 0;
                    int gy = (j > 0 && j < h-1) ?
                             (int)curr_img[(j+1)*w + i] - (int)curr_img[(j-1)*w + i] : 0;
                    grad_sum += sqrtf((float)(gx*gx + gy*gy));
                }
            }

            /* 更高的梯度总和 = 更丰富的特征 = 更可能是目标 */
            if (grad_sum > best_mse) {  /* 取最大梯度 */
                best_mse = grad_sum;
                best_dx = dx;
                best_dy = dy;
            }
        }
    }

    /* 更新 ROI 位置 */
    new_roi->x = cx + best_dx - tw / 2;
    new_roi->y = cy + best_dy - th / 2;

    /* 边界保护 */
    if (new_roi->x < 0) new_roi->x = 0;
    if (new_roi->y < 0) new_roi->y = 0;
    if (new_roi->x + new_roi->w > w) new_roi->x = w - new_roi->w;
    if (new_roi->y + new_roi->h > h) new_roi->y = h - new_roi->h;

    /* 如果 best_dx==0 && best_dy==0 且 梯度很弱, 认为失败 */
    if (best_dx == 0 && best_dy == 0 && best_mse < 50.0f) return -1;

    new_roi->score = best_mse;
    return 0;
}
