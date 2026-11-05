/* 测试替身：FreeRTOS task.h */
#ifndef TEST_STUB_TASK_H
#define TEST_STUB_TASK_H
static inline void vTaskDelete(void *t) { (void)t; }
static inline void vTaskDelay(uint32_t t) { (void)t; }
#endif
