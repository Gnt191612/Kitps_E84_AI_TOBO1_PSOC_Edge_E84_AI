/**
 * @file preprocess.c
 * @brief 图像预处理实现
 */

#include "preprocess.h"
#include <string.h>
#include <stdlib.h>

/* ──── RGB565 转灰度 ──── */
/* Gray = 0.299*R + 0.587*G + 0.114*B */
/* 使用整数运算避免浮点 */
void Preprocess_ToGrayscale(const uint8_t *src, uint8_t *dst, int len)
{
    for (int i = 0; i < len; i++) {
        uint16_t pixel = src[0] | ((uint16_t)src[1] << 8);
        /* RGB565: R[4:0]<<3, G[5:0]<<2, B[4:0]<<3 */
        uint8_t r = (pixel >> 8) & 0xF8;  /* top 5 bits, <<3 */
        uint8_t g = (pixel >> 3) & 0xFC;  /* top 6 bits, <<2 */
        uint8_t b = (pixel << 3) & 0xF8;  /* top 5 bits, <<3 */

        /* Gray = (77*R + 150*G + 29*B) >> 8 */
        dst[i] = (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
        src += 2;  /* 每个像素 2 字节 */
    }
}

/* ──── 直方图均衡 ──── */
void Preprocess_Equalize(uint8_t *img, int len)
{
    if (len <= 0) return;

    /* 统计直方图 */
    int hist[256];
    memset(hist, 0, sizeof(hist));
    for (int i = 0; i < len; i++) {
        hist[img[i]]++;
    }

    /* 累积分布 */
    int cum[256];
    int sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += hist[i];
        cum[i] = sum;
    }

    /* 映射表 */
    uint8_t map[256];
    int total = len;
    for (int i = 0; i < 256; i++) {
        map[i] = (uint8_t)((cum[i] * 255) / total);
    }

    /* 均衡化 */
    for (int i = 0; i < len; i++) {
        img[i] = map[img[i]];
    }
}

/* ──── 图像缩放 (最近邻) ──── */
void Preprocess_Scale(const uint8_t *src, int src_w, int src_h,
                      uint8_t *dst, int dst_w, int dst_h)
{
    for (int y = 0; y < dst_h; y++) {
        int src_y = (y * src_h) / dst_h;
        if (src_y >= src_h) src_y = src_h - 1;
        for (int x = 0; x < dst_w; x++) {
            int src_x = (x * src_w) / dst_w;
            if (src_x >= src_w) src_x = src_w - 1;
            dst[y * dst_w + x] = src[src_y * src_w + src_x];
        }
    }
}

/* ──── 简单二值化 (Otsu 法) ──── */
void Preprocess_Binarize(const uint8_t *img, uint8_t *out, int len)
{
    if (len <= 0) return;

    /* 统计直方图 */
    int hist[256];
    memset(hist, 0, sizeof(hist));
    for (int i = 0; i < len; i++) {
        hist[img[i]]++;
    }

    /* Otsu 阈值 */
    int total = len;
    float sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += (float)(i * hist[i]);
    }

    float sumB = 0;
    int wB = 0, wF = 0;
    float maxVar = 0;
    int threshold = 128;

    for (int t = 0; t < 256; t++) {
        wB += hist[t];
        if (wB == 0) continue;
        wF = total - wB;
        if (wF == 0) break;

        sumB += (float)(t * hist[t]);
        float meanB = sumB / wB;
        float meanF = (sum - sumB) / wF;
        float var = (float)wB * (float)wF * (meanB - meanF) * (meanB - meanF);
        if (var > maxVar) {
            maxVar = var;
            threshold = t;
        }
    }

    /* 二值化 */
    for (int i = 0; i < len; i++) {
        out[i] = (img[i] > threshold) ? 255 : 0;
    }
}
