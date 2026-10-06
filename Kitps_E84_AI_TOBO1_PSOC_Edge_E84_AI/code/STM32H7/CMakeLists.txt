cmake_minimum_required(VERSION 3.16)
set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)
set(CMAKE_CROSSCOMPILING            TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY)

set(ARM_GCC_PATH "$ENV{ARM_GCC_PATH}" CACHE PATH "Arm GNU Toolchain bin directory")
find_program(ARM_GCC NAMES arm-none-eabi-gcc HINTS "${ARM_GCC_PATH}" REQUIRED)
find_program(ARM_OBJCOPY NAMES arm-none-eabi-objcopy HINTS "${ARM_GCC_PATH}" REQUIRED)
find_program(ARM_SIZE NAMES arm-none-eabi-size HINTS "${ARM_GCC_PATH}" REQUIRED)
set(CMAKE_C_COMPILER                "${ARM_GCC}")
set(CMAKE_ASM_COMPILER              "${ARM_GCC}")
set(CMAKE_OBJCOPY                   "${ARM_OBJCOPY}")
set(CMAKE_SIZE                      "${ARM_SIZE}")

set(MCU_FLAGS
    -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
    -Os -ffunction-sections -fdata-sections -Wall -g
    -DSTM32H743xx -DUSE_HAL_DRIVER
)

project(STM32H7_RadarMaster C ASM)

include_directories(
    .  Core  Drivers  Radar  Algorithm  TargetManager
    Communication  ClosedLoop  Network  DataLogger
    Src  Inc
    Drivers/STM32H7xx_HAL_Driver/Inc
    Drivers/CMSIS/Device/ST/STM32H7xx/Include
    Drivers/CMSIS/Include
)

# 应用源文件
file(GLOB_RECURSE SRC
    Core/*.c  Radar/*.c  Algorithm/*.c  TargetManager/*.c
    Communication/*.c  ClosedLoop/*.c  DataLogger/*.c  Drivers/*.S
)

# 入口切换：使用 CubeMX 生成的 Src/main.c 作为入口（Core/main.c 已移除）
list(REMOVE_ITEM SRC ${CMAKE_SOURCE_DIR}/Core/main.c)
list(APPEND SRC ${CMAKE_SOURCE_DIR}/Src/main.c)

# Network
file(GLOB NET_SRC ${CMAKE_SOURCE_DIR}/Network/data_exchange.c ${CMAKE_SOURCE_DIR}/Network/network.c)

# I2C Master 驱动（当前未使用，注释以消除编译冲突；如需恢复请取消注释）
# list(APPEND SRC ${CMAKE_SOURCE_DIR}/Src/i2c.c)
list(APPEND SRC ${NET_SRC})

# CubeMX 生成的初始化文件
list(APPEND SRC
    ${CMAKE_SOURCE_DIR}/Src/gpio.c
)

# Drivers 自定义文件
list(APPEND SRC
    ${CMAKE_SOURCE_DIR}/Drivers/servo.c
    ${CMAKE_SOURCE_DIR}/Drivers/gimbal.c
    ${CMAKE_SOURCE_DIR}/Drivers/gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/uart.c
    ${CMAKE_SOURCE_DIR}/Drivers/timer.c
)
# CAN 驱动（未启用 FDCAN HAL，先注释；如需启用请添加 stm32h7xx_hal_fdcan.c）
# list(APPEND SRC ${CMAKE_SOURCE_DIR}/Drivers/can.c)

# HAL 库源文件
list(APPEND SRC
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c
)

add_executable(${PROJECT_NAME}.elf ${SRC})
target_compile_options(${PROJECT_NAME}.elf PRIVATE ${MCU_FLAGS})

set(LD_SCRIPT "${CMAKE_SOURCE_DIR}/STM32H743ZITx_FLASH.ld")
file(COPY ${LD_SCRIPT} DESTINATION ${CMAKE_BINARY_DIR})
target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_BINARY_DIR}/STM32H743ZITx_FLASH.ld
    -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map -nostartfiles
)
target_link_libraries(${PROJECT_NAME}.elf PRIVATE -lc -lm -lgcc)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}.elf
)
