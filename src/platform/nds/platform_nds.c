#include <nds.h>
#include "platform.h"

// Bottom-screen bitmap base, defined in graphics_nds.c and presented there.
extern color_t* g_sub_gfx;

// NDS implementation of the platform interface (see platform.h). The main 2D
// engine runs in LCDC framebuffer mode (MODE_FB0), displaying VRAM bank A directly
// as the 256x192 BGR555 image. The game renders at the LCD's native 256x192 (see
// SCREEN_WIDTH/HEIGHT in platform.h), so present_frame copies the back buffer 1:1
// over the whole screen — no scaling, no letterbox border.

void platform_init(void) {
    powerOn(POWER_ALL_2D);

    // Main engine -> direct VRAM framebuffer (MODE_FB0), bank A mapped to LCDC.
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);

    // Bottom (sub) screen: a 16-bit direct-color bitmap background (BG3, 256x256)
    // backed by VRAM bank C, so the game can draw to it through the shared
    // rasteriser the same way as the top screen (see gfx_target_sub / the bottom
    // back buffer g_subbuf in graphics.c). Master brightness stays normal — the
    // main-screen fades use REG_MASTER_BRIGHT only, so the bottom screen is
    // unaffected by menu transitions.
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    int sub_bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    g_sub_gfx = (color_t*)bgGetGfxPtr(sub_bg);
    REG_MASTER_BRIGHT_SUB = 0;
    // Clear it once (opaque black) so nothing flashes before the first present.
    for (int i = 0; i < 256 * 256; i++) g_sub_gfx[i] = 0x8000;

    // Clear the framebuffer to black/opaque once so nothing flashes before the
    // first present_frame fills it (present then writes every pixel each frame).
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) VRAM_A[i] = 0x8000;

    // Free-running timer 0 at BUS_CLOCK/1024 (~32728 Hz) for the game clock.
    timerStart(0, ClockDivider_1024, 0, NULL);
}

uint16_t platform_keys(void) {
    scanKeys();
    // libnds KEY_* share the GBA bit layout; KEY_MASK keeps the 10 game buttons.
    return (uint16_t)(keysHeld() & KEY_MASK);
}

uint16_t platform_timer(void) {
    return timerTick(0);
}

void platform_vsync(void) {
    swiWaitForVBlank();
}
