cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)
set(CMAKE_CROSSCOMPILING            TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY)

set(ARM_GCC_PATH "C:/Users/37966/Desktop/Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/arm-gnu-toolchain-13.2.rel1/arm-gnu-toolchain-13.2.Rel1-mingw-w64-i686-arm-none-eabi/bin")
set(CMAKE_C_COMPILER                "${ARM_GCC_PATH}/arm-none-eabi-gcc.exe")
set(CMAKE_ASM_COMPILER              "${ARM_GCC_PATH}/arm-none-eabi-gcc.exe")
set(CMAKE_OBJCOPY                   "${ARM_GCC_PATH}/arm-none-eabi-objcopy.exe")
set(CMAKE_OBJDUMP                   "${ARM_GCC_PATH}/arm-none-eabi-objdump.exe")
set(CMAKE_SIZE                      "${ARM_GCC_PATH}/arm-none-eabi-size.exe")

set(MCU_FLAGS
    -mcpu=cortex-m55 -march=armv8.1-m.main+mve.fp+fp.dp -DCYT3BB5CEE
    -mthumb
    -mfloat-abi=hard
    -mfpu=auto
    -Os
    -ffunction-sections
    -fdata-sections
    -Wall
    -g
    -isystem "${ARM_GCC_PATH}/../arm-none-eabi/include"
)

project(PSE84E_Project C ASM)

set(PDL_CANDIDATE_PATHS
    "${CMAKE_SOURCE_DIR}/libs/mtb-pdl-cat1"
    "${CMAKE_SOURCE_DIR}/../mtb-pdl-cat1"
    "C:/Users/37966/ModusToolbox/mtb-pdl-cat1"
    "C:/Infineon/Tools/ModusToolbox/mtb-pdl-cat1"
    "C:/Users/37966/Desktop/Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/file/mtb-pdl-cat1"
    "${CMAKE_SOURCE_DIR}/../../mtb-pdl-cat1"
)

set(PDL_ROOT "" CACHE PATH "PDL root path")
foreach(candidate ${PDL_CANDIDATE_PATHS})
    # cy_pdl.h 可能在 devices/COMPONENT_CAT1C/include/ 下
    if(EXISTS "${candidate}/devices/COMPONENT_CAT1C/include/cy_pdl.h")
        set(PDL_ROOT "${candidate}")
        break()
    endif()
    if(EXISTS "${candidate}/drivers/include/cy_pdl.h")
        set(PDL_ROOT "${candidate}")
        break()
    endif()
endforeach()

if(PDL_ROOT)
    message(STATUS "PDL found at: ${PDL_ROOT}")
    include_directories(
        "${PDL_ROOT}/drivers/include"
        "${PDL_ROOT}/devices/COMPONENT_CAT1C/include"
        "${PDL_ROOT}/devices/COMPONENT_CAT1C/include/ip"
    )
    # 如果 cy_pdl.h 在 devices/ 下，额外添加 PDL 根目录
    if(EXISTS "${PDL_ROOT}/devices/COMPONENT_CAT1C/include/cy_pdl.h" AND NOT EXISTS "${PDL_ROOT}/drivers/include/cy_pdl.h")
        include_directories("${PDL_ROOT}")
    endif()
else()
    message(WARNING "PDL (mtb-pdl-cat1) not found! Use -DPDL_ROOT=...")
endif()

include_directories(
    Core
    Communication
    Algorithm
    DataLogger
    NPU
    NPU/utils
    Device
    Drivers
    Drivers/ov7675
    Drivers/bitbanged_uart
)

file(GLOB_RECURSE SRC
    Core/*.c
    Communication/*.c
    Algorithm/*.c
    DataLogger/*.c
    NPU/*.c
    NPU/utils/*.c
    Device/*.c
    Device/startup_*.S
    Drivers/*.c
)

# 排除 i2c.c（J5 SDA/SCL 物理上未连接，接线指南明确仅使用四路 INT 线）
list(REMOVE_ITEM SRC ${CMAKE_SOURCE_DIR}/Drivers/i2c.c)

# 手动添加 ov7675 和 bb_uart 源文件（GLOB_RECURSE 不递归子目录）
list(APPEND SRC
    ${CMAKE_SOURCE_DIR}/Drivers/ov7675/ov7675.c
    ${CMAKE_SOURCE_DIR}/Drivers/bitbanged_uart/bb_uart.c
)

# 排除旧 SCB0 UART 驱动（已由位操 UART 替代）
list(REMOVE_ITEM SRC ${CMAKE_SOURCE_DIR}/Drivers/uart.c)

# 添加 PDL 源码（过滤掉 C++ 或不兼容文件）
file(GLOB PDL_SRC
    "${PDL_ROOT}/drivers/source/cy_scb_uart.c"
    "${PDL_ROOT}/drivers/source/cy_scb_spi.c"
    "${PDL_ROOT}/drivers/source/cy_scb_common.c"
    "${PDL_ROOT}/drivers/source/cy_scb_i2c.c"
    "${PDL_ROOT}/drivers/source/cy_gpio.c"
    "${PDL_ROOT}/drivers/source/cy_sysclk.c"
    "${PDL_ROOT}/drivers/source/cy_sysclk_v2.c"
    "${PDL_ROOT}/drivers/source/cy_syslib.c"

    "${PDL_ROOT}/drivers/source/cy_tcpwm_counter.c"
    "${PDL_ROOT}/drivers/source/cy_tcpwm_pwm.c"
    "${PDL_ROOT}/drivers/source/cy_sysfault.c"
    "${PDL_ROOT}/drivers/source/cy_lvd.c"
    "${PDL_ROOT}/drivers/source/cy_wdt.c"
    "${PDL_ROOT}/drivers/source/cy_syspm.c"
    "${PDL_ROOT}/drivers/source/cy_syspm_v2.c"
    "${PDL_ROOT}/drivers/source/cy_flash.c"
)
list(APPEND SRC ${PDL_SRC})

add_executable(${PROJECT_NAME}.elf ${SRC})
target_compile_options(${PROJECT_NAME}.elf PRIVATE ${MCU_FLAGS})

file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/Device/XMC8400E_1024KB_FLASH.ld" LINKER_SCRIPT)
target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    "-T${LINKER_SCRIPT}"
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    -nostdlib
    -lgcc
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}.elf
)
