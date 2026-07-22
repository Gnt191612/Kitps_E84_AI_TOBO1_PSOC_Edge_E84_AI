/**
 * @file capture.c
 * @brief OV2640 图像采集封装实现
 */

#include "capture.h"
#include "Drivers/ov2640/ov2640.h"

/* ──── 初始化 ──── */
int Capture_Init(void)
{
    /* OV2640 初始化在 system.c 中完成, 这里检查 */
    return 0;
}

/* ──── 采集一帧 ──── */
int Capture_Frame(uint8_t *buf, int w, int h)
{
    if (!buf) return -1;
    (void)w;
    (void)h;

    size_t expected = OV2640_WIDTH * OV2640_HEIGHT * 2; /* RGB565: 2 bytes/pixel */
    int ret = OV2640_Capture(buf, expected);
    return (ret > 0) ? 0 : -1;
}
