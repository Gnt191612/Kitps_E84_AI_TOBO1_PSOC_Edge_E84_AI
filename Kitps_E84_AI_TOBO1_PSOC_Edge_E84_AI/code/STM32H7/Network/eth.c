/**
 * @file    eth.c
 * @brief   以太网初始化 —— 当前未使用（保留骨架）
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║ ⚠ 此文件为保留骨架，当前编译/运行未启用。                       ║
 * ║                                                               ║
 * ║ 原因：STM32H743ZIT6 内部集成 ETH MAC，但需要外接 PHY 芯片      ║
 * ║       (如 LAN8720) 通过 RMII 接口才能工作。当前竞赛系统        ║
 * ║       未配备 PHY，所有板间通信走 UART，上位机通过 ESP32-A      ║
 * ║       的 WiFi WebSocket 中继。                                 ║
 * ║                                                               ║
 * ║ 如需启用：                                                     ║
 * ║   1. CubeMX 中开启 ETH + lwIP                                 ║
 * ║   2. 接 PHY 芯片至 RMII 引脚                                  ║
 * ║   3. 在 CMakeLists.txt 中加入此文件                            ║
 * ║   4. 在 System_Init() 中调用 ETH_Init()                        ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include "eth.h"
#include "main.h"
#include "logger.h"

extern ETH_HandleTypeDef heth;

void ETH_Init(void)
{
    /* 当前未使用：工程未集成 PHY 芯片，也未链接 lwIP */
    /* 若将来启用，请在 CubeMX 中生成 MX_LWIP_Init() 后取消注释： */
    // MX_LWIP_Init();
    Logger_Print(LOG_INFO, "Ethernet stub — not used (no PHY on board).");
}