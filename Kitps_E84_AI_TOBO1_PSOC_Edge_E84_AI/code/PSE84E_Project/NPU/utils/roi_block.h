#ifndef ROI_BLOCK_H
#define ROI_BLOCK_H

#include <stdint.h>
#include <stdbool.h>

#define IMG_W       160
#define IMG_H       120
#define BLOCK_SIZE  16    // 每个区块 16x16
#define TOP_BLOCKS  3     // 选特征值最高的3个区块

typedef struct {
    int x;
    int y;
    int w;
    int h;
    float score;
} BlockROI_t;

void ROI_InitBlocks(int img_w, int img_h, int block_size);
void ROI_CalcBlockScores(uint8_t *img, BlockROI_t *out_blocks);
void ROI_GetTopBlocks(BlockROI_t *blocks, BlockROI_t *top_blocks);

#endif