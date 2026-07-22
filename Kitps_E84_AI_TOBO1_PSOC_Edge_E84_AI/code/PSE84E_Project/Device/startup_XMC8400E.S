/**
 * @file    startup_XMC8400E.S
 * @brief   PSoC Edge E84 (XMC8400E, Cortex-M55) 启动代码
 *
 * 功能：
 *   1. 中断向量表（含初始栈顶指针 + 全部异常入口）
 *   2. Reset_Handler: 复制 .data → SRAM、清零 .bss、调用 SystemInit → main
 *
 * 编译参数匹配 CMakeLists.txt：
 *   -mcpu=cortex-m55 -mfloat-abi=hard -mfpu=auto
 *
 * 内存映射（来自链接脚本）：
 *   FLASH:  0x10000000 (1024 KB)
 *   SRAM:   0x30000000 (256 KB)
 *   栈顶:   _estack (由链接脚本提供)
 */

.syntax unified
.arch armv8.1-m.main
.fpu fpv5-d16
.thumb

/* ========================================================================== */
/* 链接脚本导出的外部符号                                                     */
/* ========================================================================== */
.extern _estack             /* 栈顶地址 (由 .ld 提供) */
.extern _sdata              /* .data 在 SRAM 中的起始地址 */
.extern _edata              /* .data 在 SRAM 中的结束地址 */
.extern _sidata             /* .data 在 FLASH 中的源地址 */
.extern _sbss               /* .bss 起始地址 */
.extern _ebss               /* .bss 结束地址 */
.extern SystemInit          /* system_XMC8400E.c 中的系统初始化 */
.extern main                /* 用户主函数 */

/* ========================================================================== */
/* 中断向量表（放置在 .vectors 段，由链接脚本放置在 FLASH 起始地址）          */
/* ========================================================================== */
.section .vectors, "a", %progbits
.type g_pfnVectors, %object
.global g_pfnVectors

g_pfnVectors:
    /* 前两个是强制项：栈顶指针 + 复位入口 */
    .word   _estack                     /* 0x10000000: 初始栈顶 */
    .word   Reset_Handler               /* 0x10000004: 复位入口 */

    /* Cortex-M 系统异常 (NMI = -2 至 SysTick = -12 共 14 个) */
    .word   NMI_Handler                 /* 0x10000008: NMI           */
    .word   HardFault_Handler           /* 0x1000000C: Hard Fault    */
    .word   MemManage_Handler           /* 0x10000010: MemManage     */
    .word   BusFault_Handler            /* 0x10000014: Bus Fault     */
    .word   UsageFault_Handler          /* 0x10000018: Usage Fault   */
    .word   0                           /* 0x1000001C: 保留          */
    .word   0                           /* 0x10000020: 保留          */
    .word   0                           /* 0x10000024: 保留          */
    .word   0                           /* 0x10000028: 保留          */
    .word   SVC_Handler                 /* 0x1000002C: SVCall        */
    .word   DebugMon_Handler            /* 0x10000030: Debug Monitor */
    .word   0                           /* 0x10000034: 保留          */
    .word   PendSV_Handler              /* 0x10000038: PendSV        */
    .word   SysTick_Handler             /* 0x1000003C: SysTick       */

    /* 外设中断 (IRQ 0 ~ IRQ 127, 共 128 个 — 扩展到覆盖 SCB 中断) */
    /* SCB0 = IRQ 18 (现有 UART), SCB3 = IRQ 105 (J5 I2C) */
    .rept   18
    .word   Default_Handler
    .endr
    .word   scb_0_interrupt_handler      /* IRQ 18: SCB0 (保留 UART)   */
    .rept   2
    .word   Default_Handler              /* IRQ 19~20                */
    .endr
    .word   ioss_interrupt_gpio_dpslp_handler /* IRQ 21: GPIO (4 INT 线) */
    .rept   83
    .word   Default_Handler              /* IRQ 22~104               */
    .endr
    .word   scb_3_interrupt_handler      /* IRQ 105: SCB3 I2C (J5)   */
    .rept   22
    .word   Default_Handler              /* IRQ 106~127              */
    .endr

g_pfnVectors_End:

.size g_pfnVectors, . - g_pfnVectors

/* ========================================================================== */
/* 复位入口                                                                   */
/* ========================================================================== */
.section .text.Reset_Handler, "ax", %progbits
.global Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    /* --------------------------------------------------------------- */
    /* 1. 设置栈指针（从链接脚本获取 _estack）                          */
    /* --------------------------------------------------------------- */
    ldr     r0, =_estack
    msr     msp, r0

    /* --------------------------------------------------------------- */
    /* 2. 复制 .data 段：FLASH(_sidata) → SRAM(_sdata ~ _edata)        */
    /* --------------------------------------------------------------- */
    ldr     r1, =_sidata        /* 源地址 (FLASH)   */
    ldr     r2, =_sdata         /* 目标地址 (SRAM)   */
    ldr     r3, =_edata         /* 目标结束地址      */
    subs    r3, r2              /* r3 = 数据长度     */
    ble     zero_bss            /* 长度为0则跳过     */

copy_data:
    /* 按字复制 (4字节/次) */
    lsrs    r3, #2              /* r3 = 字数         */
    beq     copy_data_bytes     /* 不足4字节转字节复制 */

copy_data_word:
    ldr     r0, [r1], #4        /* 从 FLASH 加载 1 字 */
    str     r0, [r2], #4        /* 写入 SRAM         */
    subs    r3, #1
    bne     copy_data_word

copy_data_bytes:
    /* 处理剩余字节 (若有) */
    ldr     r3, =_edata
    subs    r3, r2              /* 剩余字节数        */
    beq     zero_bss

copy_data_byte_loop:
    ldrb    r0, [r1], #1
    strb    r0, [r2], #1
    subs    r3, #1
    bne     copy_data_byte_loop

    /* --------------------------------------------------------------- */
    /* 3. 清零 .bss 段 (_sbss ~ _ebss)                                 */
    /* --------------------------------------------------------------- */
zero_bss:
    ldr     r2, =_sbss
    ldr     r3, =_ebss
    subs    r3, r2
    ble     call_system_init

    movs    r0, #0

zero_bss_word:
    lsrs    r3, #2
    beq     zero_bss_bytes

zero_bss_word_loop:
    str     r0, [r2], #4
    subs    r3, #1
    bne     zero_bss_word_loop

zero_bss_bytes:
    ldr     r3, =_ebss
    subs    r3, r2
    beq     call_system_init

zero_bss_byte_loop:
    strb    r0, [r2], #1
    subs    r3, #1
    bne     zero_bss_byte_loop

    /* --------------------------------------------------------------- */
    /* 4. 调用 SystemInit → main                                       */
    /* --------------------------------------------------------------- */
call_system_init:
    bl      SystemInit

    /* 跳转到主函数 */
    bl      main

    /* main 返回后死循环 */
    b       .

.size Reset_Handler, . - Reset_Handler

/* ========================================================================== */
/* 默认中断处理 (弱符号，可被外设驱动中的同名强符号覆盖)                       */
/* ========================================================================== */
.section .text.Default_Handler, "ax", %progbits
.global Default_Handler
.global NMI_Handler
.global HardFault_Handler
.global MemManage_Handler
.global BusFault_Handler
.global UsageFault_Handler
.global SVC_Handler
.global DebugMon_Handler
.global PendSV_Handler
.global SysTick_Handler

.type Default_Handler, %function
.type NMI_Handler, %function
.type HardFault_Handler, %function
.type MemManage_Handler, %function
.type BusFault_Handler, %function
.type UsageFault_Handler, %function
.type SVC_Handler, %function
.type DebugMon_Handler, %function
.type PendSV_Handler, %function
.type SysTick_Handler, %function

Default_Handler:
NMI_Handler:
HardFault_Handler:
MemManage_Handler:
BusFault_Handler:
UsageFault_Handler:
SVC_Handler:
DebugMon_Handler:
PendSV_Handler:
SysTick_Handler:
    /* 死循环 + 断点友好 */
    bkpt    0
    b       .

.size Default_Handler, . - Default_Handler

/* ========================================================================== */
/* 弱 Symbols：允许链接时被其他文件中的实现覆盖                                 */
/* ========================================================================== */
.weak   NMI_Handler
.weak   HardFault_Handler
.weak   MemManage_Handler
.weak   BusFault_Handler
.weak   UsageFault_Handler
.weak   SVC_Handler
.weak   DebugMon_Handler
.weak   PendSV_Handler
.weak   SysTick_Handler
.weak   scb_0_interrupt_handler
.weak   ioss_interrupt_gpio_dpslp_handler
.weak   scb_3_interrupt_handler

/* ========================================================================== */
/* 文件结束                                                                   */
/* ========================================================================== */
.end
