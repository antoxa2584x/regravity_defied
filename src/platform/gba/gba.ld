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
        . = ALIGN(4);   /* pad so .data and .iwram.text LMAs are word-aligned */
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

    /* Large, never-zeroed buffers (the frame back buffer) live in EWRAM.
       NOLOAD: not in the ROM image and skipped by the crt0 BSS clear — the
       back buffer is fully overwritten by clear_screen every frame anyway. */
    .ewram (NOLOAD) : {
        *(.ewram*)
    } > ewram

    /* Hot physics code placed in IWRAM (0 wait states) after BSS.
       LMA in ROM (after .data); copied to IWRAM at boot by crt0.s. */
    .iwram.text : {
        __iwram_code_start = .;
        *(.iwram.text*)
        __iwram_code_end = .;
    } > iwram AT > rom

    __data_load = LOADADDR(.data);
    __data_start = ADDR(.data);
    __data_end = ADDR(.data) + SIZEOF(.data);

    __iwram_code_load = LOADADDR(.iwram.text);
    __iwram_code_start_sym = ADDR(.iwram.text);
    __iwram_code_end_sym = ADDR(.iwram.text) + SIZEOF(.iwram.text);
}
