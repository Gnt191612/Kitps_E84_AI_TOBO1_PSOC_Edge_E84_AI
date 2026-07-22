/**
 * @file feature.c
 * @brief 特征提取：梯度计算实现
 */

#include "feature.h"
#include <math.h>
#include <string.h>

/* ──── Sobel 3x3 梯度 ──── */
void Feature_CalcGradient(const uint8_t *img, int w, int h, Gradient_t *grad_buf)
{
    if (!img || !grad_buf || w < 3 || h < 3) return;

    /* 初始化边界为 0 */
    memset(grad_buf, 0, w * h * sizeof(Gradient_t));

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = y * w + x;

            /* Sobel X: [[-1,0,1],[-2,0,2],[-1,0,1]] */
            int gx = -img[(y-1)*w + (x-1)] + img[(y-1)*w + (x+1)]
                     -2*img[y*w + (x-1)]    + 2*img[y*w + (x+1)]
                     -img[(y+1)*w + (x-1)]  + img[(y+1)*w + (x+1)];

            /* Sobel Y: [[-1,-2,-1],[0,0,0],[1,2,1]] */
            int gy = -img[(y-1)*w + (x-1)] -2*img[(y-1)*w + x] - img[(y-1)*w + (x+1)]
                     +img[(y+1)*w + (x-1)] +2*img[(y+1)*w + x] + img[(y+1)*w + (x+1)];

            grad_buf[idx].gx = (float)gx;
            grad_buf[idx].gy = (float)gy;
            grad_buf[idx].mag = sqrtf((float)(gx*gx + gy*gy));
            grad_buf[idx].ang = atan2f((float)gy, (float)gx);
        }
    }
}

/* ──── 快速梯度幅值 (|gx|+|gy|) ──── */
void Feature_CalcGradientMag(const uint8_t *img, int w, int h, uint8_t *mag)
{
    if (!img || !mag || w < 3 || h < 3) return;

    memset(mag, 0, w * h);

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int gx = -img[(y-1)*w + (x-1)] + img[(y-1)*w + (x+1)]
                     -2*img[y*w + (x-1)]    + 2*img[y*w + (x+1)]
                     -img[(y+1)*w + (x-1)]  + img[(y+1)*w + (x+1)];

            int gy = -img[(y-1)*w + (x-1)] -2*img[(y-1)*w + x] - img[(y-1)*w + (x+1)]
                     +img[(y+1)*w + (x-1)] +2*img[(y+1)*w + x] + img[(y+1)*w + (x+1)];

            int mag_val = abs(gx) + abs(gy);
            if (mag_val > 255) mag_val = 255;
            mag[y * w + x] = (uint8_t)mag_val;
        }
    }
}

/* ──── 区域平均梯度幅值 ──── */
float Feature_RegionMeanMag(const Gradient_t *grad_buf, int w, int h,
                            int x, int y, int rw, int rh)
{
    if (!grad_buf) return 0.0f;

    /* 边界裁剪 */
    if (x < 0) { rw += x; x = 0; }
    if (y < 0) { rh += y; y = 0; }
    if (x + rw > w) rw = w - x;
    if (y + rh > h) rh = h - y;
    if (rw <= 0 || rh <= 0) return 0.0f;

    float sum = 0.0f;
    int count = 0;
    for (int j = y; j < y + rh; j++) {
        for (int i = x; i < x + rw; i++) {
            sum += grad_buf[j * w + i].mag;
            count++;
        }
    }
    return (count > 0) ? (sum / count) : 0.0f;
}
