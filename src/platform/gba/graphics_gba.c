#include "gba.h"
#include "graphics.h"

// GBA display backend: presents the shared back buffer (g_backbuf, defined in
// graphics.c) to the Mode 3 bitmap, and does the hardware brightness fades. The
// portable rasteriser primitives live in graphics.c.

// Source word for DMA fill — must live in RAM (IWRAM via .bss).
static uint32_t dma_fill_word;

void clear_screen(color_t color) {
    dma_fill_word = color | ((uint32_t)color << 16);
    REG_DMA3SAD   = (uint32_t)&dma_fill_word;
    REG_DMA3DAD   = (uint32_t)g_backbuf;
    REG_DMA3CNT_L = SCREEN_WIDTH * SCREEN_HEIGHT / 2; // 32-bit word count
    REG_DMA3CNT_H = DMA_ENABLE_32_SRCFIX;              // start immediately, halts CPU until done
}

// Blit the back buffer to VRAM. Call right after VBlank starts: the DMA copies
// linearly and outruns the scan beam, so the whole frame appears tear-free.
//
// Copied in horizontal bands instead of one blocking transfer: a full-screen
// DMA blocks the bus for ~6ms, far longer than the DirectSound FIFO's ~1ms of
// buffer, which would make a playing SFX glitch every frame. The sound FIFO DMA
// (DMA1) outranks DMA3, so it refills between bands; each band is ~0.3ms, well
// inside the FIFO budget.
void present_frame(void) {
    enum { BANDS = 20, ROWS = SCREEN_HEIGHT / BANDS };  // 8 rows per band
    for (int b = 0; b < BANDS; b++) {
        int off = b * ROWS * SCREEN_WIDTH;              // offset in pixels
        REG_DMA3SAD   = (uint32_t)(g_backbuf + off);
        REG_DMA3DAD   = VRAM + off * 2;                 // VRAM offset in bytes
        REG_DMA3CNT_L = SCREEN_WIDTH * ROWS / 2;        // 32-bit word count
        REG_DMA3CNT_H = DMA_ENABLE_32;
    }
}

// ~6 VBlanks per direction: snappy but visibly smooth. The blend is a free
// hardware post-effect, so the framebuffer is untouched during the fade.
#define FADE_STEP 3

void fade_out(void) {
    REG_BLDCNT = BLD_BG2 | BLD_DARKEN;
    for (int y = 0; y < 16; y += FADE_STEP) { vsync(); REG_BLDY = y; }
    vsync(); REG_BLDY = 16;
}

void fade_in(void) {
    REG_BLDCNT = BLD_BG2 | BLD_DARKEN;
    for (int y = 16; y > 0; y -= FADE_STEP) { vsync(); REG_BLDY = y; }
    vsync();
    REG_BLDY = 0;
    REG_BLDCNT = 0;   // disable the blend so normal display is unaffected
}
