# build_all.ps1 — 构建 H7 和 ESP32，并提示 84E 官方工程入口

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  多板协作目标跟踪系统 — 一键编译脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 定位脚本所在目录
$ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($env:ARM_GCC_PATH) { $env:PATH = "$env:ARM_GCC_PATH;$env:PATH" }
foreach ($tool in @("cmake", "ninja", "arm-none-eabi-gcc")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "未找到 $tool，请将其加入 PATH；Arm 工具链也可通过 ARM_GCC_PATH 指定。"
    }
}

# ========== 1. KIT_PSE84_AI ==========
Write-Host "`n[1/3] 跳过 KIT_PSE84_AI 旧兼容工程" -ForegroundColor Yellow
Write-Host "  PSE84E_Project 仅用于应用层编译检查，不得烧录。" -ForegroundColor Yellow
Write-Host "  请使用 Workspace\PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision。" -ForegroundColor Yellow

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
    idf.py set-target esp32s3 2>&1 | Out-Null
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
