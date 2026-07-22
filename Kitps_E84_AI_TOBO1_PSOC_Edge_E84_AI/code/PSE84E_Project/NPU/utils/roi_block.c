#include "roi_block.h"
#include <string.h>

static int block_cols;
static int block_rows;

// 初始化区块网�?
void ROI_InitBlocks(int img_w, int img_h, int block_size) {
    block_cols = img_w / block_size;
    block_rows = img_h / block_size;
}

// 计算单个区块的特征值（用梯度强度）
static float CalcBlockScore(uint8_t *img, int img_w, int img_h, int bx, int by, int block_size) {
    int sum = 0;
    for (int y = by; y < by + block_size; y++) {
        for (int x = bx; x < bx + block_size; x++) {
            if (x > 0 && x < img_w - 1 && y > 0 && y < img_h - 1) {
                int dx = abs(img[y * img_w + (x + 1)] - img[y * img_w + (x - 1)]);
                int dy = abs(img[(y + 1) * img_w + x] - img[(y - 1) * img_w + x]);
                sum += dx + dy;
            }
        }
    }
    return (float)sum / (block_size * block_size);
}

// 计算所有区块的特征�?
void ROI_CalcBlockScores(uint8_t *img, BlockROI_t *out_blocks) {
    int idx = 0;
    for (int r = 0; r < block_rows; r++) {
        for (int c = 0; c < block_cols; c++) {
            int bx = c * BLOCK_SIZE;
            int by = r * BLOCK_SIZE;
            float score = CalcBlockScore(img, IMG_W, IMG_H, bx, by, BLOCK_SIZE);
            out_blocks[idx++] = (BlockROI_t){bx, by, BLOCK_SIZE, BLOCK_SIZE, score};
        }
    }
}

// 选出特征值最高的TOP3区块
void ROI_GetTopBlocks(BlockROI_t *blocks, BlockROI_t *top_blocks) {
    memset(top_blocks, 0, sizeof(BlockROI_t) * TOP_BLOCKS);
    for (int i = 0; i < block_cols * block_rows; i++) {
        for (int j = 0; j < TOP_BLOCKS; j++) {
            if (blocks[i].score > top_blocks[j].score) {
                // 后移腾出位置
                for (int k = TOP_BLOCKS - 1; k > j; k--) {
                    top_blocks[k] = top_blocks[k - 1];
                }
                top_blocks[j] = blocks[i];
                break;
            }
        }
    }
}

