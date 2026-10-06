@echo off
chcp 65001 >nul
echo ========================================
echo   多板协作目标跟踪系统 — 编译脚本
echo ========================================
echo.

set ROOT=%~dp0

where cmake >nul 2>&1 || (echo ^(FAIL^) 未找到 CMake & exit /b 1)
where ninja >nul 2>&1 || (echo ^(FAIL^) 未找到 Ninja & exit /b 1)
where arm-none-eabi-gcc >nul 2>&1 || (
    if defined ARM_GCC_PATH (
        set PATH=%ARM_GCC_PATH%;%PATH%
    ) else (
        echo ^(FAIL^) 未找到 arm-none-eabi-gcc，请加入 PATH 或设置 ARM_GCC_PATH
        exit /b 1
    )
)

REM ======== 1. KIT_PSE84_AI ========
echo [1/3] 跳过 KIT_PSE84_AI 旧兼容工程
echo   ^(SKIP^) PSE84E_Project 仅用于应用层编译检查，不得烧录
echo   请使用 Workspace\PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision
echo   中 TARGET_APP_KIT_PSE84_AI 的官方 CM33/CM55 多核构建

REM ======== 2. STM32H7 ========
echo [2/3] 编译 STM32H7 ...
cd /d "%ROOT%STM32H7"
if exist build rmdir /s /q build
mkdir build
cd build
cmake .. -G "Ninja" >nul 2>&1
if %errorlevel%==0 (
    ninja >nul 2>&1
    if %errorlevel%==0 (
        echo   ^(OK^) STM32H7 编译成功
        for %%f in (*.hex) do copy /y "%%f" "..\..\STM32H7_Main.hex" >nul
        for %%f in (*.elf) do copy /y "%%f" "..\..\STM32H7_Main.elf" >nul
        for %%f in (*.bin) do copy /y "%%f" "..\..\STM32H7_Main.bin" >nul
    ) else (
        echo   ^(FAIL^) STM32H7 编译失败
    )
) else (
    echo   ^(SKIP^) 缺少 HAL 库，请在 CubeMX 中生成
)
cd "%ROOT%"

REM ======== 3. ESP32 ========
echo [3/3] 编译 ESP32 ^(需要 ESP-IDF^) ...
cd /d "%ROOT%ESP32_ov2640"
where idf.py >nul 2>&1
if %errorlevel%==0 (
    call idf.py build >nul 2>&1
    if %errorlevel%==0 (
        echo   ^(OK^) ESP32 编译成功
        if exist build\ESP32_ov2640.bin copy /y build\ESP32_ov2640.bin ..\..\ESP32_Tracker.bin >nul
    ) else (
        echo   ^(FAIL^) ESP32 编译失败
    )
) else (
    echo   ^(SKIP^) 未检测到 ESP-IDF 环境
)
cd "%ROOT%"

echo.
echo ========================================
echo   编译完成！
echo ========================================
echo.
echo 复制到 code\ 根目录下的产物：
echo   STM32H7_Main.hex    - 烧录到 STM32H743ZIT6 Cortex-M7 主控板
echo   STM32H7_Main.elf    - 调试用 ELF
echo   ESP32_Tracker.bin   - 烧录到双 ESP32（各烧一块，代码相同）
echo.
echo 注意：
echo   - STM32H7: 用 STM32CubeProgrammer 通过 ST-Link 烧录
echo   - KIT_PSE84_AI: 仅使用官方 Workspace 的多核镜像和编程流程
echo   - ESP32:   idf.py -p COMx flash
echo.
pause
