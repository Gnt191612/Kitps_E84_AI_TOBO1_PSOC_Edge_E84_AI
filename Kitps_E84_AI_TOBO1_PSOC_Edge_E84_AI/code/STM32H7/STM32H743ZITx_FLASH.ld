/* STM32H743ZITx_FLASH.ld - GCC Linker Script */
ENTRY(Reset_Handler)

_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
    FLASH (rx)    : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
    RAM_D1 (xrw)  : ORIGIN = 0x24000000, LENGTH = 512K
    RAM_D2 (xrw)  : ORIGIN = 0x30000000, LENGTH = 288K
    RAM_D3 (xrw)  : ORIGIN = 0x38000000, LENGTH = 64K
    ITCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text : { *(.text*) *(.glue_7) *(.glue_7t) *(.eh_frame) } > FLASH
    .rodata : { *(.rodata*) } > FLASH
    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : { *(.ARM.exidx*) } > FLASH
    .preinit_array : { KEEP(*(.preinit_array*)) } > FLASH
    .init_array : { KEEP(*(SORT(.init_array*))) } > FLASH
    .fini_array : { KEEP(*(SORT(.fini_array*))) } > FLASH

    _sidata = .;
    .data : AT(_sidata)
    {
        _sdata = .; *(.data*) _edata = .;
    } > RAM_D1

    .bss :
    {
        _sbss = .; *(.bss*) *(COMMON) _ebss = .;
    } > RAM_D1

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(_end = .);
        PROVIDE(end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM_D1
}

PROVIDE(_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1));
