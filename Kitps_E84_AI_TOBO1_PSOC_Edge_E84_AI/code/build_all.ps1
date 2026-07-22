# build_all.ps1 — 一键编译三个项目
# 使用前先确认 Deepcraft Studio 的 ARM GCC 路径在 CMakeLists.txt 中配置正确
# 确认 Infineon PDL (mtb-pdl-cat1) 已安装
# ESP32 需要 ESP-IDF v5.x 环境，先执行 idf.py set-target esp32

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  多板协作目标跟踪系统 — 一键编译脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 定位脚本所在目录
$ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
$ARM_GCC = "C:/Users/37966/Desktop/Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/file/arm-gnu-toolchain-12.3.rel1-mingw-w64-i686-arm-none-eabi/bin"
$env:PATH = "$ARM_GCC;$env:PATH"

# ========== 1. 编译 PSE84E (NPU识别) ==========
Write-Host "`n[1/3] 编译 PSE84E ..." -ForegroundColor Green
Push-Location "$ROOT\PSE84E_Project"
if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
New-Item -ItemType Directory -Path "build" -Force | Out-Null
Set-Location build
cmake .. -G "Ninja" 2>&1
if ($LASTEXITCODE -eq 0) {
    ninja 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✅ PSE84E 编译成功!" -ForegroundColor Green
        if (Test-Path "PSE84E_Project.hex") {
            Copy-Item "PSE84E_Project.hex" "$ROOT\..\PSE84E_NPU.hex" -Force
        }
    } else {
        Write-Host "  ❌ PSE84E 编译失败" -ForegroundColor Red
    }
} else {
    Write-Host "  ⚠️  PSE84E CMake 配置失败，可能是缺少 PDL 库" -ForegroundColor Yellow
    Write-Host "     请检查 CMakeLists.txt 中的 PDL_ROOT 路径" -ForegroundColor Yellow
}
Pop-Location

# ========== 2. 编译 STM32H7 (主控) ==========
Write-Host "`n[2/3] 编译 STM32H7 ..." -ForegroundColor Green
Push-Location "$ROOT\STM32H7"
if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
New-Item -ItemType Directory -Path "build" -Force | Out-Null
Set-Location build
cmake .. -G "Ninja" 2>&1
if ($LASTEXITCODE -eq 0) {
    ninja 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✅ STM32H7 编译成功!" -ForegroundColor Green
        if (Test-Path "STM32H7_RadarMaster.hex") {
            Copy-Item "STM32H7_RadarMaster.hex" "$ROOT\..\STM32H7_Main.hex" -Force
        }
    } else {
        Write-Host "  ❌ STM32H7 编译失败" -ForegroundColor Red
    }
} else {
    Write-Host "  ⚠️  STM32H7 CMake 配置失败，可能是缺少 HAL 库" -ForegroundColor Yellow
    Write-Host "     请先在 CubeMX 中按盈的引脚表配置并生成代码" -ForegroundColor Yellow
}
Pop-Location

# ========== 3. 编译 ESP32 (视觉跟踪 × 2) ==========
Write-Host "`n[3/3] 编译 ESP32 (需要 ESP-IDF 环境) ..." -ForegroundColor Green
Push-Location "$ROOT\ESP32_ov2640"
$has_idf = Get-Command idf.py -ErrorAction SilentlyContinue
if ($has_idf) {
    idf.py set-target esp32 2>&1 | Out-Null
    idf.py build 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✅ ESP32 编译成功!" -ForegroundColor Green
        if (Test-Path "build/ESP32_ov2640.bin") {
            Copy-Item "build/ESP32_ov2640.bin" "$ROOT\..\ESP32_Tracker.bin" -Force
        }
        Write-Host "  📌 烧录两块 ESP32，代码相同，物理区分" -ForegroundColor Cyan
    } else {
        Write-Host "  ❌ ESP32 编译失败" -ForegroundColor Red
    }
} else {
    Write-Host "  ⚠️  未检测到 ESP-IDF，跳过 ESP32 编译" -ForegroundColor Yellow
    Write-Host "     ESP32 需要在安装了 ESP-IDF v5.x 的机器上编译" -ForegroundColor Yellow
    Write-Host "     命令: idf.py build" -ForegroundColor Yellow
}
Pop-Location

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  编译完成" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan
