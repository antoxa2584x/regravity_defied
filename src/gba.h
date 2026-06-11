#ifndef GBA_H
#define GBA_H

#include <stdint.h>

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

// Video mode 3: 240x160, 16-bit color
#define MODE3 0x0003
#define BG2_ENABLE 0x0400

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

// Key masks
#define KEY_A (1 << 0)
#define KEY_B (1 << 1)
#define KEY_SELECT (1 << 2)
#define KEY_START (1 << 3)
#define KEY_RIGHT (1 << 4)
#define KEY_LEFT (1 << 5)
#define KEY_UP (1 << 6)
#define KEY_DOWN (1 << 7)
#define KEY_R (1 << 8)
#define KEY_L (1 << 9)

// Helper to create a color (5 bits per channel)
#define COLOR(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

typedef uint16_t color_t;

// Big Endian Helpers
static inline uint32_t read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline uint16_t read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

// Wait for vblank START so rendering begins at scanline 160 (not scanline 0).
// Old code waited for vblank END, causing all drawing to happen during active
// display — the source of flickering.
static inline void vsync() {
    while (REG_VCOUNT >= 160);  // if already in vblank, wait for active display
    while (REG_VCOUNT < 160);   // wait for next vblank start
}

// Mark a function to run from IWRAM (0 wait states, 32-bit bus).
// Functions placed here must be copied from ROM at startup (see crt0.s).
#define IWRAM_FN __attribute__((section(".iwram.text"), noinline))

#endif // GBA_H
