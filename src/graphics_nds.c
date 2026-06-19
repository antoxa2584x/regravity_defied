#include <nds.h>
#include "graphics.h"

// NDS display backend. The DS main engine runs in LCDC framebuffer mode
// (MODE_FB0), displaying VRAM bank A directly as a 256x192 BGR555 image. The game
// renders at the native 256x192 into the shared back buffer (g_backbuf, defined
// in graphics.c); present_frame() copies it 1:1 over the whole framebuffer — no
// scaling, no letterbox border.
//
// On the DS, framebuffer bit 15 is the per-pixel "opaque" flag — pixels with it
// clear show as backdrop. The game's COLOR() values leave it 0, so present sets
// it on every pixel as it copies.

void clear_screen(color_t color) {
    color_t* b = g_backbuf;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) b[i] = color;
}

void present_frame(void) {
    // Copy the back buffer over the whole LCDC framebuffer, setting the DS opaque
    // bit (15) on every pixel. SCREEN_WIDTH (256) is even and the buffers 4-byte
    // aligned, so this runs as 32-bit writes — two pixels per store, ORing
    // 0x80008000 — to keep the per-frame copy cheap enough to comfortably sustain
    // 60 fps on the ARM9.
    u32* fb = (u32*)VRAM_A;   // bank A mapped to the LCDC framebuffer
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        const u32* src = (const u32*)(g_backbuf + y * SCREEN_WIDTH);
        u32* dst = fb + (y * SCREEN_WIDTH) / 2;
        for (int x = 0; x < SCREEN_WIDTH / 2; x++)
            dst[x] = src[x] | 0x80008000u;
    }
}

// Hardware master-brightness fades, mirroring the GBA BLDY darken effect.
// REG_MASTER_BRIGHT: bits 0..4 = factor (0..16), bits 14..15 = mode (2 = down).
#define BRIGHT_DOWN(f) ((2 << 14) | (f))
#define FADE_STEP 3

void fade_out(void) {
    for (int y = 0; y < 16; y += FADE_STEP) { swiWaitForVBlank(); REG_MASTER_BRIGHT = BRIGHT_DOWN(y); }
    swiWaitForVBlank(); REG_MASTER_BRIGHT = BRIGHT_DOWN(16);
}

void fade_in(void) {
    for (int y = 16; y > 0; y -= FADE_STEP) { swiWaitForVBlank(); REG_MASTER_BRIGHT = BRIGHT_DOWN(y); }
    swiWaitForVBlank();
    REG_MASTER_BRIGHT = 0;   // normal brightness
}
