#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

// Portable core shared by every build target (GBA, NDS, host harness). It holds
// only things that are the same everywhere — the logical screen, the color and
// button model, big-endian level helpers — plus the small platform interface
// (platform_init/keys/timer/vsync) that each target implements in its own
// platform_<target>.c. All GBA hardware registers live in gba.h, which includes
// this file; the NDS backend pulls in <nds.h> instead.

// Logical screen the game renders to — the size of the back buffer every drawing
// primitive targets. On the GBA (and the host test harness) this is the native
// Mode 3 bitmap, 240x160. On the NDS we render at the LCD's full 256x192 instead,
// so the frame fills the screen 1:1 with no scaling or letterbox border: the same
// world-space code just projects a wider/taller view (more track to the edges).
// On the 3DS we render at the top screen's full 400x240 so the playfield fills it
// edge to edge (no side bars); the 320-wide bottom screen shows the centred 320px
// window of that buffer (see graphics_3ds.c), and since every dual-screen layout
// is centred on SCREEN_WIDTH/2 nothing important is cropped there.
// Physics is screen-independent, so showing more of the world changes nothing but
// the visible area. libnds also defines SCREEN_WIDTH/HEIGHT (the 256x192 LCD), so
// we take over those names for the game's logical resolution.
#ifdef SCREEN_WIDTH
#undef SCREEN_WIDTH
#endif
#ifdef SCREEN_HEIGHT
#undef SCREEN_HEIGHT
#endif
#if defined(PLATFORM_3DS)
#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240
#elif defined(PLATFORM_NDS)
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#else
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
#endif

// Targets with a second physical screen (the DS and 3DS bottom screens) share the
// dual-screen game layout: the level-select detail card and the in-game progress
// minimap live on the bottom screen instead of being squeezed onto the playfield.
// Shared code keys off DUAL_SCREEN so both backends light up the same paths.
#if defined(PLATFORM_NDS) || defined(PLATFORM_3DS)
#define DUAL_SCREEN 1
#endif

// Color: 15-bit BGR, 5 bits per channel — the format shared by the GBA Mode 3
// bitmap and the NDS LCDC framebuffer. Bit 15 is unused here; the NDS present
// path sets it (on DS it is the per-pixel "opaque" bit).
#define COLOR(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

typedef uint16_t color_t;

// Logical button bitmask (the GBA layout). The calico/libnds KEY_* macros use
// exactly these bit values, so when an NDS backend file includes <nds.h> first
// this block is skipped and the libnds definitions are used unchanged.
#ifndef KEY_A
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
#endif

// Big-endian helpers — the embedded levels.mrg is stored big-endian.
static inline uint32_t read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline uint16_t read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

// --- Per-target code/data placement and wall-clock rate --------------------
// IWRAM_FN marks the hot rasteriser/physics functions; EWRAM_BSS marks the
// large frame back buffer. On the GBA these map to the fast on-chip RAM banks;
// on the NDS the ARM9 caches make plain main RAM fast enough, so both are no-ops
// (the GBA .iwram.text/.ewram sections don't exist in the DS linker script).
#if defined(HOST_BUILD)
  #define IWRAM_FN
  #define EWRAM_BSS
  #define CLOCK_HZ 16384
#elif defined(PLATFORM_3DS)
  #define IWRAM_FN
  #define EWRAM_BSS
  // The RTC millisecond clock (osGetTime) is scaled up to the game clock in
  // platform_3ds.c; keep the rate a clean power of two for the timing derivations
  // below (TICKS_PER_STEP = 32768/60 etc.).
  #define CLOCK_HZ 32768
#elif defined(PLATFORM_NDS)
  #define IWRAM_FN
  #define EWRAM_BSS
  // Free-running DS timer at BUS_CLOCK/1024 = 33.51 MHz / 1024 ≈ 32728 Hz.
  #define CLOCK_HZ 32728
#else
  // GBA: hot code in IWRAM (0 wait states, 32-bit ARM bus), big buffers in EWRAM.
  #define IWRAM_FN __attribute__((section(".iwram.text"), noinline, target("arm")))
  #define EWRAM_BSS __attribute__((section(".ewram")))
  // TM0 at 16.78 MHz / 1024 = 16384 Hz.
  #define CLOCK_HZ 16384
#endif

// Timing derived from the platform timer rate, so the game keeps the same
// real-time speed on both clocks. With CLOCK_HZ 16384 these are the GBA's
// original literals (273 / 8192 / 1638 / 2048).
#define TICKS_PER_STEP (CLOCK_HZ / 60)  // one 60 Hz fixed-timestep sim step
#define BLINK_TICKS    (CLOCK_HZ / 2)   // 0.5 s blink half-period
#define CHECKER_TICKS  (CLOCK_HZ / 10)  // checkered-flag wave step
#define FLOAT_TICKS    (CLOCK_HZ / 8)   // splash-title float step

// --- Platform interface (implemented per target in platform_<target>.c) ----
// Bring up video, the free-running game-clock timer, and any memory tuning.
void platform_init(void);
// Currently-held buttons as a logical bitmask (KEY_*; 1 = pressed).
uint16_t platform_keys(void);
// Free-running ~CLOCK_HZ counter, 16-bit (wraps at 65536), for the game clock.
uint16_t platform_timer(void);
// Block until the next vertical blank; paces the render loop.
void platform_vsync(void);

// --- Stereoscopic 3D (3DS only) --------------------------------------------
// The 3DS top screen is autostereoscopic. The game is 2D, so depth is faked by
// shifting each render layer horizontally between the two eyes (a layered
// pop-up-book effect): the track sits at the screen plane, the bike floats a
// little toward the viewer, and the flags pop the most. See render_gameplay in
// main.c and present_frame_top in graphics_3ds.c.
#if defined(PLATFORM_3DS)
// px of parallax at full 3D slider, per layer (all pop toward the viewer; larger
// = closer). The track is the play surface and pops least; the bike rides just in
// front of it (its shadow stays glued to the track depth); the flags pop most.
// Text (HUD + menus) pops a touch in front of the track so every glyph reads in
// depth. The track is not a flat card: the far (ribbon) edge of the isometric
// surface recedes STEREO_DEPTH_RIBBON behind its near (riding) edge, so the ribbon
// slopes away into the screen rather than floating as one plane (see draw_track).
#define STEREO_DEPTH_TRACK  3
#define STEREO_DEPTH_BIKE   5
#define STEREO_DEPTH_FLAG   8
#define STEREO_DEPTH_TEXT   4
#define STEREO_DEPTH_RIBBON 5
// Horizontal parallax in px for a layer at `depth`, scaled by the 3D slider.
int  stereo_px(int depth);
// Nonzero when the 3D slider is off zero (the playfield is worth rendering twice).
int  stereo_active(void);
// Present g_backbuf to the top screen's left (eye 0) or right (eye 1) framebuffer.
void present_frame_top(int eye);
#else
// On mono targets the parallax collapses to zero at compile time, so the shared
// render path is identical to the original single-view code.
#define stereo_px(depth) 0
#endif

// --- mGBA debug logging (GBA + -DDEBUG only) -------------------------------
// Output appears in mGBA's log as [GBA:DEBUG] lines. No-op on NDS/host.
#if defined(DEBUG) && !defined(HOST_BUILD) && !defined(PLATFORM_NDS) && !defined(PLATFORM_3DS)
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

#endif // PLATFORM_H
