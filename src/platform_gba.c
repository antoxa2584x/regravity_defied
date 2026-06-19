#include "gba.h"

// GBA implementation of the platform interface (see platform.h).

void platform_init(void) {
    // Enable ROM prefetch buffer: sequential ROM reads drop from 4 to 1 wait
    // state. Bit 14 = prefetch enable; rest are sensible wait state values.
    REG_WAITCNT = 0x4317;

    // EWRAM to 1 wait state (BIOS default is 2). The frame back buffer lives in
    // EWRAM and is both drawn into and DMA-copied to VRAM each frame, so this
    // ~33% RAM speedup directly buys back the cost of double-buffering.
    REG_MEMCTL = 0x0E000020;

    // Free-running timer 0 at 16384 Hz drives the fixed-timestep loop.
    REG_TM0CNT_L = 0;
    REG_TM0CNT_H = TM_ENABLE | TM_PRESCALE_1024;

    // Mode 3: 240x160 16-bit bitmap on BG2.
    REG_DISPCNT = MODE3 | BG2_ENABLE;
}

uint16_t platform_keys(void) {
    // KEYINPUT is active-low; invert and keep the 10 game buttons.
    return ~REG_KEYINPUT & 0x03FF;
}

uint16_t platform_timer(void) {
    return REG_TM0CNT_L;
}

void platform_vsync(void) {
    vsync();
}
