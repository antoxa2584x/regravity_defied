#ifndef GBA_H
#define GBA_H

#include "platform.h"

// GBA hardware register map. This header is GBA-only: it is included by the
// bare-metal GBA backends (platform_gba.c, graphics_gba.c, sound_gba.c,
// save_gba.c) and by the generated asset headers. Everything portable lives in
// platform.h, which this file pulls in so existing `#include "gba.h"` sites keep
// seeing color_t, COLOR, the KEY_* masks and the read_be helpers.

// GBA Register Definitions
#define REG_DISPCNT  (*(volatile uint32_t*)0x04000000)
#define REG_VCOUNT   (*(volatile uint16_t*)0x04000006)
#define REG_KEYINPUT (*(volatile uint16_t*)0x04000130)
#define REG_WAITCNT  (*(volatile uint16_t*)0x04000204)
#ifndef VRAM
#define VRAM 0x06000000
#endif

// DMA channel 3 — general-purpose, used for fast screen fill
#define REG_DMA3SAD   (*(volatile uint32_t*)0x040000D4)
#define REG_DMA3DAD   (*(volatile uint32_t*)0x040000D8)
#define REG_DMA3CNT_L (*(volatile uint16_t*)0x040000DC)
#define REG_DMA3CNT_H (*(volatile uint16_t*)0x040000DE)
// DMACNT_H bits: enable(15) | 32-bit(10) | src-fixed(8-7=10)
#define DMA_ENABLE_32_SRCFIX 0x8500
// Same, but src + dst both increment — used for the back-buffer -> VRAM blit.
#define DMA_ENABLE_32 0x8400

// Timer 0 — free-running clock for the fixed-timestep game loop.
#define REG_TM0CNT_L (*(volatile uint16_t*)0x04000100)
#define REG_TM0CNT_H (*(volatile uint16_t*)0x04000102)
// Timer 1 — sample clock for DirectSound (Timer 0 is the game clock).
#define REG_TM1CNT_L (*(volatile uint16_t*)0x04000104)
#define REG_TM1CNT_H (*(volatile uint16_t*)0x04000106)
#define TM_ENABLE     0x0080
#define TM_PRESCALE_1024 0x0003

// --- DirectSound (channel A) + DMA1 feeding its FIFO ---------------------
#define REG_SOUNDCNT_L (*(volatile uint16_t*)0x04000080)
#define REG_SOUNDCNT_H (*(volatile uint16_t*)0x04000082)
#define REG_SOUNDCNT_X (*(volatile uint16_t*)0x04000084)
#define REG_SOUNDBIAS  (*(volatile uint16_t*)0x04000088)
#define REG_FIFO_A     0x040000A0
#define REG_DMA1SAD   (*(volatile uint32_t*)0x040000BC)
#define REG_DMA1DAD   (*(volatile uint32_t*)0x040000C0)
#define REG_DMA1CNT_L (*(volatile uint16_t*)0x040000C4)
#define REG_DMA1CNT_H (*(volatile uint16_t*)0x040000C6)
// DMA1 control for FIFO: enable | special-timing | 32-bit | repeat | dest-fixed.
#define DMA_SOUND_FIFO 0xB640

// Internal memory control. Writing 0x0E000020 sets EWRAM to 1 wait state
// (vs the BIOS default 2), ~33% faster RAM — matters because the back buffer
// lives in EWRAM and is both drawn into and DMA-copied out every frame.
#define REG_MEMCTL (*(volatile uint32_t*)0x04000800)

// Video mode 3: 240x160, 16-bit color
#define MODE3 0x0003
#define BG2_ENABLE 0x0400

// Brightness blend, used for screen fades. BLDCNT selects the target layer and
// effect; BLDY is the 0..16 coefficient (0 = normal, 16 = fully black/white).
#define REG_BLDCNT (*(volatile uint16_t*)0x04000050)
#define REG_BLDY   (*(volatile uint16_t*)0x04000054)
#define BLD_BG2     0x0004   // BG2 (the MODE3 bitmap) is the 1st-target layer
#define BLD_DARKEN  0x00C0   // brightness-decrease mode (fade toward black)

// Wait for vblank START so rendering begins at scanline 160 (not scanline 0).
// Old code waited for vblank END, causing all drawing to happen during active
// display — the source of flickering.
static inline void vsync(void) {
    while (REG_VCOUNT >= 160);  // if already in vblank, wait for active display
    while (REG_VCOUNT < 160);   // wait for next vblank start
}

#endif // GBA_H
