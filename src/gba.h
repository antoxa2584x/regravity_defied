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
// Timer ticks at 16.78MHz/1024 = 16384 Hz, so one 60 Hz sim step = 273 ticks.
#define TICKS_PER_STEP 273

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
// target("arm") emits 32-bit ARM instructions, fully utilising the 32-bit
// IWRAM bus (Thumb only uses 16 of the 32 bits per fetch cycle).
// Functions placed here must be copied from ROM at startup (see crt0.s).
#ifdef HOST_BUILD
// Host harness (x86) can't honour the ARM-specific placement attributes.
#define IWRAM_FN
#else
#define IWRAM_FN __attribute__((section(".iwram.text"), noinline, target("arm")))
#endif

// mGBA debug logging — compiled in only when -DDEBUG is passed.
// Output appears in mGBA's log as [GBA:DEBUG] lines.
#ifdef DEBUG
#define MGBA_DEBUG_STR  ((volatile char*)0x04FFF600)
#define MGBA_DEBUG_OUT  (*(volatile uint16_t*)0x04FFF700)
#define MGBA_DEBUG_INIT (*(volatile uint16_t*)0x04FFF780)

static inline void debug_init(void) {
    MGBA_DEBUG_INIT = 0xC0DE;
}

static inline void debug_log(const char* tag, int val) {
    volatile char* b = MGBA_DEBUG_STR;
    int i = 0;
    while (tag[i] && i < 200) { b[i] = tag[i]; i++; }
    if (val < 0) { b[i++] = '-'; val = -val; }
    char tmp[12]; int n = 0;
    if (!val) tmp[n++] = '0';
    while (val > 0) { tmp[n++] = '0' + val % 10; val /= 10; }
    while (n > 0) b[i++] = tmp[--n];
    b[i] = 0;
    MGBA_DEBUG_OUT = 0x100 | 3; // level 3 = info (same as DMA messages, not filtered)
}
#else
#define debug_init() ((void)0)
#define debug_log(tag, val) ((void)0)
#endif

#endif // GBA_H
