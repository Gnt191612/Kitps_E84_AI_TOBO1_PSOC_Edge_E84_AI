/* 测试替身：FreeRTOS（h7_link.c 用到的部分） */
#ifndef TEST_STUB_FREERTOS_H
#define TEST_STUB_FREERTOS_H
#include <stdint.h>
#define configMAX_PRIORITIES  8
#define pdMS_TO_TICKS(ms)     (ms)
#endif
