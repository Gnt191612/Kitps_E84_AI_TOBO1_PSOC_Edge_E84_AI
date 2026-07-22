/**
 * @file    XMC8400E_1024KB_FLASH.ld
 * @brief   链接脚本 - PSoC Edge E84 (XMC8400E)
 *
 * 内存布局：
 *   FLASH:  0x32011000 - 0x32069FFF (实际芯片映射)
 *   SRAM:   0x30000000 - 0x3003FFFF (256 KB)
 *
 * 入口：Reset_Handler
 *
 * GCC ARM Embedded 链接脚本
 */

/* ============================ 内存定义 ============================ */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x32011000, LENGTH = 1024K
    SRAM  (rwx) : ORIGIN = 0x30000000, LENGTH = 256K
}

/* ============================ 栈和堆配置 ========================== */
__stack_size   = 0x00004000;   /* 16 KB 栈        */
__heap_size    = 0x00008000;   /* 32 KB 堆        */

/* ============================ 段定义 ============================== */
SECTIONS
{
    /* ---- 中断向量表和代码 ---- */
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))          /* 中断向量表   */
        __Vectors_End = .;
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)                   /* 代码         */
        *(.text*)
        *(.glue_7)                 /* Thumb interworking */
        *(.glue_7t)
        *(.gnu.linkonce.t.*)

        /* 构造函数/析构函数 */
        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;                /* 代码段结束   */
    } > FLASH

    /* ---- 只读数据 ---- */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
    } > FLASH

    /* ---- ARM 异常表 ---- */
    .ARM.extab :
    {
        *(.ARM.extab*)
        *(.gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    /* ---- 预初始化数据（需要在 .data 复制前执行的指针） ---- */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    /* ---- 初始化数据 (复制到 SRAM) ---- */
    _sidata = .;                   /* FLASH 中 .data 的源地址 */

    .data : AT(_sidata)
    {
        . = ALIGN(4);
        _sdata = .;                /* SRAM 中 .data 的起始地址 */
        *(.data)
        *(.data*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(4);
        _edata = .;                /* SRAM 中 .data 的结束地址 */
    } > SRAM

    /* ---- 未初始化数据 ---- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;                 /* BSS 起始 */
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;                 /* BSS 结束 */
        __bss_end__ = _ebss;
    } > SRAM

    /* ---- 堆 ---- */
    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        PROVIDE(__heap_start__ = .);
        . = . + __heap_size;
        . = ALIGN(8);
        PROVIDE(__heap_end__ = .);
    } > SRAM

    /* ---- 栈 ---- */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        PROVIDE(__stack_start__ = .);
        . = . + __stack_size;
        . = ALIGN(8);
        _estack = .;               /* 栈顶（栈向下增长） */
        PROVIDE(__stack_end__ = _estack);
    } > SRAM

    /* ---- 丢弃的段 ---- */
    /DISCARD/ :
    {
        libc.a (*)
        libm.a (*)
        libgcc.a (*)
    }
}

/* ============================ 导出符号 ============================ */
PROVIDE(__flash_start = ORIGIN(FLASH));
PROVIDE(__flash_end   = ORIGIN(FLASH) + LENGTH(FLASH));
PROVIDE(__sram_start  = ORIGIN(SRAM));
PROVIDE(__sram_end    = ORIGIN(SRAM) + LENGTH(SRAM));
