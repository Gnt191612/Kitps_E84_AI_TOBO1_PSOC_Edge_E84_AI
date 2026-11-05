/**
 * @file    console.c
 * @brief   调试串口命令 + 可选停机按键实现（轮询，非阻塞）
 */

#include "console.h"
#include "main.h"
#include "scheduler.h"
#include "logger.h"
#include <string.h>
#include <ctype.h>

#define CONSOLE_LINE_MAX  32

static char    s_line[CONSOLE_LINE_MAX];
static uint8_t s_len = 0;

#if CONSOLE_BUTTON_ENABLE
static uint8_t  s_btn_stable = 1;     /* 1 = 松开（上拉） */
static uint8_t  s_btn_last = 1;
static uint32_t s_btn_change_tick = 0;
#endif

void Console_Init(void)
{
    s_len = 0;
#if CONSOLE_BUTTON_ENABLE
    GPIO_InitTypeDef g = {0};
    g.Pin = CONSOLE_BUTTON_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CONSOLE_BUTTON_PORT, &g);
#endif
    Logger_Print(LOG_INFO, "Console ready: STOP / START / STATUS / RELEARN");
}

static void ToggleRun(void)
{
    if (Scheduler_IsPaused()) {
        Scheduler_Resume();
    } else {
        Scheduler_RequestShutdown();
    }
}

static void Execute(const char *cmd)
{
    if (strcmp(cmd, "STOP") == 0) {
        Logger_Print(LOG_INFO, "CMD STOP: returning all gimbals to initial pose");
        Scheduler_RequestShutdown();
    } else if (strcmp(cmd, "START") == 0) {
        Logger_Print(LOG_INFO, "CMD START");
        Scheduler_Resume();
    } else if (strcmp(cmd, "STATUS") == 0) {
        Scheduler_PrintStatus();
    } else if (strcmp(cmd, "RELEARN") == 0) {
        Logger_Print(LOG_INFO, "CMD RELEARN: keep the scan area empty until background is learned");
        Scheduler_RelearnBackground();
    } else if (cmd[0] != '\0') {
        Logger_Print(LOG_WARN, "Unknown cmd '%s' (STOP/START/STATUS/RELEARN)", cmd);
    }
}

void Console_Poll(void)
{
    /* UART4 只在此处轮询读取，不开中断。已启用 16 字节接收 FIFO，
     * 一整行命令（如 "STATUS\r\n"）可在两轮主循环之间缓存；溢出时清除 ORE 继续 */
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        char c = (char)(huart4.Instance->RDR & 0xFF);
        if (c == '\r' || c == '\n') {
            s_line[s_len] = '\0';
            Execute(s_line);
            s_len = 0;
        } else if (s_len < CONSOLE_LINE_MAX - 1) {
            s_line[s_len++] = (char)toupper((unsigned char)c);
        }
    }
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart4);
    }

#if CONSOLE_BUTTON_ENABLE
    uint32_t now = HAL_GetTick();
    uint8_t raw = (HAL_GPIO_ReadPin(CONSOLE_BUTTON_PORT, CONSOLE_BUTTON_PIN) == GPIO_PIN_SET) ? 1 : 0;
    if (raw != s_btn_last) {
        s_btn_last = raw;
        s_btn_change_tick = now;
    } else if (raw != s_btn_stable && (now - s_btn_change_tick) >= 50U) {
        s_btn_stable = raw;
        if (raw == 0) ToggleRun();   /* 按下沿 */
    }
#else
    (void)ToggleRun;
#endif
}
