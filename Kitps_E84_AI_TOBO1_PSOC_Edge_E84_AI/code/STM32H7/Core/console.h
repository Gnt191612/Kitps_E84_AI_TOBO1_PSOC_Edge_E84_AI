/**
 * @file    console.h
 * @brief   调试串口命令 + 可选停机按键（实现开题“系统停止时全部回到初始朝向”）
 *
 * UART4（PC10 TX / PC11 RX，921600 8N1）输入一行命令，回车结束：
 *   STOP    所有 ESP32 释放、雷达与三路云台回到初始朝向，调度暂停
 *   START   从暂停恢复（雷达背景保留，不必重新学习）
 *   STATUS  打印目标数、各 ESP32 角色和雷达背景是否学好
 *
 * 可选实体按键：把 CONSOLE_BUTTON_ENABLE 改为 1 并填写引脚，
 * 按下（低电平有效、50 ms 去抖）在 STOP / START 之间切换。
 */
#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 可选停机按键（按底板实际按键填写后再启用） ---- */
#ifndef CONSOLE_BUTTON_ENABLE
#define CONSOLE_BUTTON_ENABLE   0
#endif
#define CONSOLE_BUTTON_PORT     GPIOC        /* 示例，需按底板修改 */
#define CONSOLE_BUTTON_PIN      GPIO_PIN_13  /* 示例，需按底板修改 */

void Console_Init(void);
/* 每轮主循环调用一次，非阻塞 */
void Console_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONSOLE_H */
