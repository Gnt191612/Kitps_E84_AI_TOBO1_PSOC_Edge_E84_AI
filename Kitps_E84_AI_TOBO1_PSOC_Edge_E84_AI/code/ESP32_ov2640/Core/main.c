/**
 * @file main.c
 * @brief ESP32_ov2640 主入口
 *
 * 入口 main(), 调用 System_Init() 后进入 while(1) 循环调用 Scheduler_Run()。
 * 使用 Xtensa LX6 双核:
 *   - Core 0: 协议/UART (在 uart_rx task 中)
 *   - Core 1: 主调度器/跟踪算法 (在 app_main 中, FreeRTOS 默认在 Core 1 运行)
 */

#include "main.h"
#include "Core/system.h"

void app_main(void)
{
    /* 系统初始化 */
    System_Init();

    /* 主循环: 每帧采集 → 处理 → 发送 */
    while (1) {
        Scheduler_Run();
    }
}
