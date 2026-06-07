OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    rom (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
}

SECTIONS {
    .text : {
        KEEP(*(.text.prologue))
        *(.text*)
        *(.rodata*)
    } > rom

    .data : {
        *(.data*)
    } > iwram AT > rom

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > iwram

    __data_load = LOADADDR(.data);
    __data_start = ADDR(.data);
    __data_end = ADDR(.data) + SIZEOF(.data);
}
