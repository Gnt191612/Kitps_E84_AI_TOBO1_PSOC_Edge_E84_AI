# Src/ 目录说明（CubeMX 生成）

本目录是 STM32CubeMX 根据 `STM32H7_RadarMaster.ioc` 生成的文件。**只有下面两个文件参与编译**（见 `../CMakeLists.txt`）：

| 文件 | 是否编译 | 说明 |
|---|---|---|
| `main.c` | 是 | 程序入口：`System_Init()` → `Scheduler_Init()` → 循环 `Scheduler_Run()` |
| `gpio.c` | 是 | GPIO 初始化。RST84E(PC1) 已改为不驱动，重新生成后需保留 |
| `stm32h7xx_it.c` | **否** | 实际使用 `../Core/stm32h7xx_it.c` |
| `stm32h7xx_hal_msp.c` | **否** | 实际使用 `../Core/stm32h7xx_hal_msp.c` |
| `system_stm32h7xx.c` | **否** | 实际使用 `../Core/system_stm32h7xx.c` |
| `tim.c` / `usart.c` / `i2c.c` | **否** | 初始化函数体已搬到 `../Core/init_wrappers.c` |

**重要**：中断服务函数、NVIC 使能、外设 MSP 初始化必须写在 `Core/` 下对应的文件里。写在本目录的同名文件中不会生效。曾经因此导致串口接收和雷达回波中断从未工作。

启动文件使用 `../Startup/startup_stm32h743zitx.s`（ST 标准，向量表完整）。

CubeMX 重新生成代码后，需要把新的初始化函数体对照搬到 `Core/init_wrappers.c`，并检查 `Core/stm32h7xx_hal_msp.c` 中的中断使能是否还在。
