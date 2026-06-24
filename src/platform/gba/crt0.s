.section .text.prologue
.global _start
.arm

_start:
    b reset

@ GBA Header (partial/simple)
.org 0x04
.space 156 @ ROM Logo (156 bytes, overwritten by gbafix.py post-build)
.space 12  @ Game Title
.space 4   @ Game Code
.space 2   @ Maker Code
.byte 0x96 @ Fixed value
.byte 0x00 @ Unit code
.byte 0x00 @ Device type
.space 7   @ Reserved
.byte 0x00 @ Software version
.byte 0x00 @ Complement check
.space 2   @ Reserved

reset:
    mov r0, #0x12 @ Switch to IRQ mode
    msr cpsr_c, r0
    ldr sp, =0x03007FA0 @ Set IRQ stack

    mov r0, #0x1F @ Switch to System mode
    msr cpsr_c, r0
    ldr sp, =0x03007F00 @ Set System stack

    @ Copy data from ROM to IWRAM
    ldr r0, =__data_load
    ldr r1, =__data_start
    ldr r2, =__data_end
copy_data:
    cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo copy_data

    @ Clear BSS
    ldr r0, =0
    ldr r1, =__bss_start
    ldr r2, =__bss_end
clear_bss:
    cmp r1, r2
    strlo r0, [r1], #4
    blo clear_bss

    @ Copy IWRAM code (physics hot path) from ROM to IWRAM
    ldr r0, =__iwram_code_load
    ldr r1, =__iwram_code_start_sym
    ldr r2, =__iwram_code_end_sym
copy_iwram:
    cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo copy_iwram

    ldr r0, =main
    bx r0

infinite_loop:
    b infinite_loop
