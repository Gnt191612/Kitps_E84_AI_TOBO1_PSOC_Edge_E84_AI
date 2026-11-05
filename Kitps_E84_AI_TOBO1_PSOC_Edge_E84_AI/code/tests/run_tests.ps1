# 电脑端测试：编译并运行全部测试（Windows / MinGW gcc）
# 用法：在 code/tests 目录下运行  powershell -ExecutionPolicy Bypass -File run_tests.ps1
# Linux / macOS 请用同目录的 run_tests.sh
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$build = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $build | Out-Null
$env:TMP = $build; $env:TEMP = $build   # gcc 需要可写的临时目录

$H7  = '../STM32H7'
$ESP = '../ESP32_ov2640'
$CM55 = '../../Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/proj_cm55/source'
$CFLAGS = @('-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Wno-unused-parameter', '-I.')

$H7INC = @('-Istubs', '-Istubs/h7', "-I$H7", "-I$H7/Core", "-I$H7/Drivers", "-I$H7/Radar",
           "-I$H7/Algorithm", "-I$H7/TargetManager", "-I$H7/Communication", "-I$H7/ClosedLoop",
           "-I$H7/Network", "-I$H7/Datalogger",
           '-include', 'stubs/h7/main.h',
           '-D__CORE_MAIN_H', '-D__TIM_H', '-D__GPIO_H', '-D__MAIN_H', '-D__TIM_H__', '-D__GPIO_H__')

function Build($name, $flags, $srcs) {
    $out = Join-Path $build "$name.exe"
    & gcc @CFLAGS @flags @srcs '-o' $out '-lm'
    if ($LASTEXITCODE -ne 0) { throw "build failed: $name" }
    return $out
}

function BuildObj($name, $flags, $src) {
    $out = Join-Path $build "$name.o"
    & gcc @CFLAGS @flags '-c' $src '-o' $out
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $src" }
    return $out
}

$tests = @()

$tests += Build 'test_h7_scheduler' $H7INC @(
    'test_h7_scheduler.c', 'stubs/h7_stubs.c',
    "$H7/Core/scheduler.c", "$H7/Drivers/gimbal.c", "$H7/Drivers/servo.c",
    "$H7/TargetManager/target_list.c", "$H7/TargetManager/target_state.c", "$H7/TargetManager/target_switch.c",
    "$H7/ClosedLoop/closed_loop.c", "$H7/ClosedLoop/relock_logic.c", "$H7/ClosedLoop/assist_localize.c",
    "$H7/Algorithm/data_fusion.c", "$H7/Algorithm/geometry.c", "$H7/Algorithm/motion_predict.c",
    "$H7/Algorithm/kalman_filter.c", "$H7/Datalogger/exp_log.c")

# 整机流程：调度器 + 真实雷达模块，同时生成 build/demo_flow.log 供监控界面回放
$tests += Build 'test_h7_demo_flow' ($H7INC + @('-DSTUB_REAL_RADAR')) @(
    'test_h7_demo_flow.c', 'stubs/h7_stubs.c',
    "$H7/Core/scheduler.c", "$H7/Drivers/gimbal.c", "$H7/Drivers/servo.c",
    "$H7/Radar/radar_process.c", "$H7/Radar/radar_map.c",
    "$H7/TargetManager/target_list.c", "$H7/TargetManager/target_state.c", "$H7/TargetManager/target_switch.c",
    "$H7/ClosedLoop/closed_loop.c", "$H7/ClosedLoop/relock_logic.c", "$H7/ClosedLoop/assist_localize.c",
    "$H7/Algorithm/data_fusion.c", "$H7/Algorithm/geometry.c", "$H7/Algorithm/motion_predict.c",
    "$H7/Algorithm/kalman_filter.c", "$H7/Datalogger/exp_log.c")

$tests += Build 'test_h7_units' $H7INC @(
    'test_h7_units.c', "$H7/Radar/radar_map.c", "$H7/Radar/radar_driver.c", "$H7/Algorithm/geometry.c")

$ESPSRC = @("$ESP/tracking/tracker.c", "$ESP/tracking/closed_loop.c", "$ESP/tracking/relock.c",
            "$ESP/algorithm/track_algorithm.c", "$ESP/algorithm/filter.c", "$ESP/algorithm/predict.c",
            "$ESP/Vision/roi/roi_select.c", "$ESP/Vision/feature.c")
$tests += Build 'test_esp32_tracker_roi3' @("-I$ESP", '-DEXP_TRACK_FULL_FRAME=0', '-DTRACK_TIME_MS=test_time_ms') (@('test_esp32_tracker.c') + $ESPSRC)
$tests += Build 'test_esp32_tracker_full' @("-I$ESP", '-DEXP_TRACK_FULL_FRAME=1', '-DTRACK_TIME_MS=test_time_ms') (@('test_esp32_tracker.c') + $ESPSRC)

# 三板通信互通：ESP32 侧与 H7 同名的函数改名后单独编译
$ESP_RENAME = @('-DProtocol_Init=ESP_Protocol_Init', '-DProtocol_ParseByte=ESP_Protocol_ParseByte',
                '-DProtocol_SendTrackResult=ESP_Protocol_SendTrackResult', '-DProtocol_SendAssistPos=ESP_Protocol_SendAssistPos',
                '-DProtocol_GetHandle=ESP_Protocol_GetHandle', '-DProtocol_GetCommStats=ESP_Protocol_GetCommStats', '-DProtocol_RegisterUploadCallback=ESP_Protocol_RegisterUploadCallback',
                '-DFrame_Pack=ESP_Frame_Pack', '-DFrame_Unpack=ESP_Frame_Unpack', '-DFrame_CalcXor=ESP_Frame_CalcXor',
                '-DFrame_Verify=ESP_Frame_Verify', '-DUART_Send=ESP_UART_Send')
$ESPFLAGS = @('-Istubs/esp', "-I$ESP") + $ESP_RENAME
$objs = @(
    (BuildObj 'esp_side'     $ESPFLAGS 'esp_side.c'),
    (BuildObj 'esp_frame'    $ESPFLAGS "$ESP/Communication/frame.c"),
    (BuildObj 'esp_protocol' $ESPFLAGS "$ESP/Communication/protocol.c"),
    (BuildObj 'e84_side'     @('-Istubs/e84', "-I$CM55") 'e84_side.c'))
$tests += Build 'test_protocol' $H7INC (@('test_protocol.c', "$H7/Communication/frame.c", "$H7/Communication/protocol.c") + $objs)

$failed = 0
foreach ($t in $tests) {
    & $t
    if ($LASTEXITCODE -ne 0) { $failed++ }
}
if ($failed) { Write-Host "$failed test program(s) FAILED" -ForegroundColor Red; exit 1 }
Write-Host 'ALL TESTS PASSED' -ForegroundColor Green
