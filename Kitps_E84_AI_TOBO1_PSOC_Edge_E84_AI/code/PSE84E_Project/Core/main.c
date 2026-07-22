/**
 * @file    main.c
 * @brief   PSE84E 主入口
 */

#include "main.h"
#include "system.h"
#include "scheduler.h"

int main(void)
{
    System_Init();
    Scheduler_Init();

    while (1)
    {
        Scheduler_Run();
    }
}