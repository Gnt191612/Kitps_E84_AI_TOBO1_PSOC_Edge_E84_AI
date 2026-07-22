@echo off
chcp 65001 >nul
echo ========================================
echo   多板协作目标跟踪系统 — 编译脚本
echo ========================================
echo.

REM ---- 工具链路径 ----
REM 使用 13.2 版本工具链（12.3 版本也保留在桌面，但推荐用新版）
set ARM_GCC_13=C:\Users\37966\Desktop\Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\arm-gnu-toolchain-13.2.rel1\arm-gnu-toolchain-13.2.Rel1-mingw-w64-i686-arm-none-eabi\bin
set ARM_GCC_12=C:\Users\37966\Desktop\Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\arm-gnu-toolchain-12.3.rel1-mingw-w64-i686-arm-none-eabi\bin
set PATH=%ARM_GCC_13%;%ARM_GCC_12%;%PATH%

set ROOT=%~dp0

REM ======== 1. PSE84E ========
echo [1/3] 编译 PSE84E ...
cd /d "%ROOT%PSE84E_Project"
if exist build rmdir /s /q build
mkdir build
cd build
cmake .. -G "Ninja" >nul 2>&1
if %errorlevel%==0 (
    ninja >nul 2>&1
    if %errorlevel%==0 (
        echo   ^(OK^) PSE84E 编译成功
        for %%f in (*.hex) do copy /y "%%f" "..\..\PSE84E_NPU.hex" >nul
    ) else (
        echo   ^(FAIL^) PSE84E 编译失败
    )
) else (
    echo   ^(SKIP^) 缺少 PDL 库，请配置 PDL_ROOT
)
cd "%ROOT%"

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
echo   PSE84E_NPU.hex      - 烧录到 KIT_PSE84_ETOBO1
echo   STM32H7_Main.hex    - 烧录到 STM32H743ZIT6
echo   STM32H7_Main.elf    - 调试用 ELF
echo   ESP32_Tracker.bin   - 烧录到双 ESP32（各烧一块，代码相同）
echo.
echo 注意：
echo   - STM32H7: 用 STM32CubeProgrammer 通过 ST-Link 烧录
echo   - PSE84E:  用 ModusToolbox Programmer 或 OpenOCD
echo   - ESP32:   idf.py -p COMx flash
echo.
pause
