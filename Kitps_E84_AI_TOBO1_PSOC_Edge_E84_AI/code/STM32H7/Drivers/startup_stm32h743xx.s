/**
 * @file    startup_stm32h743xx.s
 * @brief   STM32H743ZIT6 GCC startup file
 *
 * 中断向量表 + Reset_Handler 入口
 * 包含 .data 段初始化（从 FLASH 拷贝到 RAM）和 .bss 段清零
 */

.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb

/* 栈顶地址（由链接器脚本提供） */
.equ  _estack, 0x24080000  /* RAM_D1 顶部，512KB */

/* 中断向量表 */
.global g_pfnVectors
.global Default_Handler

.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* 外设中断 */
  .word 0 /* WWDG */
  .word 0 /* PVD */
  .word 0 /* TAMP */
  .word 0 /* RTC */
  .word 0 /* FLASH */
  .word 0 /* RCC */
  .word 0 /* EXTI0 */
  .word 0 /* EXTI1 */
  .word 0 /* EXTI2 */
  .word 0 /* EXTI3 */
  .word 0 /* EXTI4 */
  .word 0 /* DMA1_Stream0 */
  .word 0 /* DMA1_Stream1 */
  .word 0 /* DMA1_Stream2 */
  .word 0 /* DMA1_Stream3 */
  .word 0 /* DMA1_Stream4 */
  .word 0 /* DMA1_Stream5 */
  .word 0 /* DMA1_Stream6 */
  .word 0 /* DMA1_Stream7 */
  .word 0 /* ADC */
  .word 0 /* FDCAN1 */
  .word 0 /* FDCAN2 */
  .word 0 /* TIM1_BRK */
  .word 0 /* TIM1_UP */
  .word 0 /* TIM1_TRG_COM */
  .word 0 /* TIM1_CC */
  .word 0 /* TIM2 */
  .word 0 /* TIM3 */
  .word 0 /* TIM4 */
  .word 0 /* TIM5 */
  .word 0 /* TIM6 */
  .word 0 /* TIM7 */
  .word 0 /* TIM8_BRK */
  .word 0 /* TIM8_UP */
  .word 0 /* TIM8_TRG_COM */
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word 0 /* UART4 */
  .word 0 /* UART5 */
  .word 0 /* USART6 */
  .word 0 /* SPI1 */
  .word 0 /* SPI2 */
  .word 0 /* SPI3 */
  .word 0 /* SPI4 */
  .word 0 /* SPI5 */
  .word 0 /* SPI6 */
  .word 0 /* SAI1 */
  .word 0 /* SAI2 */
  .size g_pfnVectors, .-g_pfnVectors

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  /* 设置栈指针 */
  ldr r0, =_estack
  mov sp, r0

  /* ─── 拷贝 .data 段：从 FLASH (__etext / _sidata) → RAM (__data_start / _sdata) ─── */
  ldr r1, =_sidata
  ldr r2, =_sdata
  ldr r3, =_edata
  subs r3, r3, r2        /* r3 = .data 大小 */
  beq .L_data_done
.L_data_loop:
  ldrb r0, [r1], #1
  strb r0, [r2], #1
  subs r3, r3, #1
  bne .L_data_loop
.L_data_done:

  /* ─── 清零 .bss 段 ─── */
  ldr r1, =_sbss
  ldr r2, =_ebss
  subs r3, r2, r1
  beq .L_bss_done
  mov r0, #0
.L_bss_loop:
  strb r0, [r1], #1
  subs r3, r3, #1
  bne .L_bss_loop
.L_bss_done:

  /* 调用 SystemInit（CMSIS 系统初始化） */
  bl SystemInit

  /* 跳转到 main */
  bl main

  /* main 返回后死循环 */
  b .

.section .text.Default_Handler, "ax", %progbits
Default_Handler:
  b .

/* 中断弱别名 */
.macro weak_alias name
  .weak \name
  .thumb_set \name, Default_Handler
.endm

weak_alias NMI_Handler
weak_alias HardFault_Handler
weak_alias MemManage_Handler
weak_alias BusFault_Handler
weak_alias UsageFault_Handler
weak_alias SVC_Handler
weak_alias DebugMon_Handler
weak_alias PendSV_Handler
weak_alias SysTick_Handler
weak_alias USART1_IRQHandler
weak_alias USART2_IRQHandler
weak_alias USART3_IRQHandler

.global SystemInit
.weak SystemInit
.type SystemInit, %function
SystemInit:
  bx lr

.end
