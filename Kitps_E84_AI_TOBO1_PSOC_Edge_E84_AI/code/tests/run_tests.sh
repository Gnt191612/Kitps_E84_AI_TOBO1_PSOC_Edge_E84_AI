#!/bin/sh
# 电脑端测试（Linux / macOS，需 gcc 或 clang）：在 code/tests 目录下运行  sh run_tests.sh
# Windows 请用同目录的 run_tests.ps1
set -e
cd "$(dirname "$0")"
B=build
mkdir -p "$B"
CC=${CC:-gcc}
H7=../STM32H7
ESP=../ESP32_ov2640
CM55=../../Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/proj_cm55/source
CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter -I."
H7INC="-Istubs -Istubs/h7 -I$H7 -I$H7/Core -I$H7/Drivers -I$H7/Radar -I$H7/Algorithm -I$H7/TargetManager \
 -I$H7/Communication -I$H7/ClosedLoop -I$H7/Network -I$H7/Datalogger -include stubs/h7/main.h \
 -D__CORE_MAIN_H -D__TIM_H -D__GPIO_H -D__MAIN_H -D__TIM_H__ -D__GPIO_H__"

$CC $CFLAGS $H7INC test_h7_scheduler.c stubs/h7_stubs.c $H7/Core/scheduler.c $H7/Drivers/gimbal.c \
  $H7/Drivers/servo.c $H7/TargetManager/target_list.c $H7/TargetManager/target_state.c \
  $H7/TargetManager/target_switch.c $H7/ClosedLoop/closed_loop.c $H7/ClosedLoop/relock_logic.c \
  $H7/ClosedLoop/assist_localize.c $H7/Algorithm/data_fusion.c $H7/Algorithm/geometry.c \
  $H7/Algorithm/motion_predict.c $H7/Algorithm/kalman_filter.c $H7/Datalogger/exp_log.c \
  -o $B/test_h7_scheduler -lm

$CC $CFLAGS $H7INC test_h7_units.c $H7/Radar/radar_map.c $H7/Radar/radar_driver.c $H7/Algorithm/geometry.c \
  -o $B/test_h7_units -lm

ESPSRC="$ESP/tracking/tracker.c $ESP/tracking/closed_loop.c $ESP/tracking/relock.c $ESP/algorithm/track_algorithm.c \
 $ESP/algorithm/filter.c $ESP/algorithm/predict.c $ESP/Vision/roi/roi_select.c $ESP/Vision/feature.c"
for m in 0 1; do
  $CC $CFLAGS -I$ESP -DEXP_TRACK_FULL_FRAME=$m -DTRACK_TIME_MS=test_time_ms test_esp32_tracker.c $ESPSRC \
    -o $B/test_esp32_tracker_$m -lm
done

REN="-DProtocol_Init=ESP_Protocol_Init -DProtocol_ParseByte=ESP_Protocol_ParseByte \
 -DProtocol_SendTrackResult=ESP_Protocol_SendTrackResult -DProtocol_SendAssistPos=ESP_Protocol_SendAssistPos \
 -DProtocol_GetHandle=ESP_Protocol_GetHandle -DProtocol_RegisterUploadCallback=ESP_Protocol_RegisterUploadCallback \
 -DFrame_Pack=ESP_Frame_Pack -DFrame_Unpack=ESP_Frame_Unpack -DFrame_CalcXor=ESP_Frame_CalcXor \
 -DFrame_Verify=ESP_Frame_Verify -DUART_Send=ESP_UART_Send"
$CC $CFLAGS -Istubs/esp -I$ESP $REN -c esp_side.c -o $B/esp_side.o
$CC $CFLAGS -Istubs/esp -I$ESP $REN -c $ESP/Communication/frame.c -o $B/esp_frame.o
$CC $CFLAGS -Istubs/esp -I$ESP $REN -c $ESP/Communication/protocol.c -o $B/esp_protocol.o
$CC $CFLAGS -Istubs/e84 -I$CM55 -c e84_side.c -o $B/e84_side.o
$CC $CFLAGS $H7INC test_protocol.c $H7/Communication/frame.c $H7/Communication/protocol.c \
  $B/esp_side.o $B/esp_frame.o $B/esp_protocol.o $B/e84_side.o -o $B/test_protocol -lm

fail=0
for t in test_h7_scheduler test_h7_units test_esp32_tracker_0 test_esp32_tracker_1 test_protocol; do
  ./$B/$t || fail=$((fail+1))
done
if [ $fail -ne 0 ]; then echo "$fail test program(s) FAILED"; exit 1; fi
echo "ALL TESTS PASSED"
