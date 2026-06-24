#include <3ds.h>
#include <stdlib.h>
#include "platform.h"

// Nintendo 3DS implementation of the platform interface (see platform.h). The
// game's software rasteriser draws into the shared back buffer (g_backbuf); the
// graphics backend (graphics_3ds.c) converts and rotates it into the 3DS
// framebuffers each frame. The 3DS is the second dual-screen target alongside the
// DS, so it reuses all the DUAL_SCREEN game logic (bottom-screen detail card,
// in-game minimap, bike shadow) and additionally drives the autostereoscopic top
// screen for a layered 3D effect (see render_gameplay in main.c).

// Implemented in graphics_3ds.c. gfx3ds_clear_black fills both top framebuffers
// (incl. the centred playfield's side bars) and the bottom one with black once;
// gfx3ds_finalize flushes + swaps the framebuffers to the LCD each frame.
void gfx3ds_clear_black(void);
void gfx3ds_finalize(void);

// 3D slider position sampled by stereo_active() each gameplay frame, as 0..256
// fixed point, then consumed by stereo_px() while rendering the two eyes.
static int g_slider_q8;

void platform_init(void) {
    gfxInitDefault();
    // On a New 3DS, run the ARM11 at its full 804 MHz (and enable the L2 cache)
    // instead of the 268 MHz Old-3DS-compatible default — the software present and
    // per-eye render are CPU-bound, so this is most of the headroom for 60 fps.
    // A no-op on an Old 3DS/2DS, which simply keeps the 268 MHz clock.
    osSetSpeedupEnable(true);
    // Enable stereoscopic 3D on the top screen. With the physical slider at zero
    // both eyes are presented identically (flat 2D); as it slides up the parallax
    // applied per layer in render_gameplay separates them into depth.
    gfxSet3D(true);

    // Keep the default 24-bit BGR8 framebuffer format (present converts BGR555 ->
    // BGR8). Single buffering mirrors the GBA/NDS model: draw straight into the
    // displayed framebuffer, then gfx3ds_finalize() (flush + swap) pushes it to
    // the LCD once per frame from platform_vsync — no page flip to track here.
    gfxSetDoubleBuffering(GFX_TOP, false);
    gfxSetDoubleBuffering(GFX_BOTTOM, false);

    // Clear once so the top-screen side bars (the 320-wide playfield centred on
    // the 400-wide LCD) and any unwritten pixels start black instead of garbage.
    gfx3ds_clear_black();

    // Tear down graphics on exit (when the HOME menu asks us to quit, see
    // platform_vsync). hid/apt/fs/sdmc are brought up by libctru's default init.
    atexit(gfxExit);
}

uint16_t platform_keys(void) {
    hidScanInput();
    u32 k = hidKeysHeld();
    // libctru's A/B/SELECT/START/D-pad/R/L bits line up exactly with the game's
    // logical KEY_* layout (the GBA bit order), so the low 10 bits pass straight
    // through. (KEY_DRIGHT..KEY_DDOWN occupy the same bits as KEY_RIGHT..DOWN.)
    uint16_t g = (uint16_t)(k & 0x03FF);
    // Map the Circle Pad onto the D-pad so analog steering works too.
    if (k & KEY_CPAD_UP)    g |= KEY_DUP;
    if (k & KEY_CPAD_DOWN)  g |= KEY_DDOWN;
    if (k & KEY_CPAD_LEFT)  g |= KEY_DLEFT;
    if (k & KEY_CPAD_RIGHT) g |= KEY_DRIGHT;
    return g;
}

uint16_t platform_timer(void) {
    // Game clock from the RTC millisecond counter, not the raw CPU tick. The fixed
    // timestep (main.c) divides this by SYSCLOCK_ARM11; if it read svcGetSystemTick
    // directly, osSetSpeedupEnable's 804 MHz New-3DS clock would make the tick count
    // ~3x faster and the game run ~3x too fast. osGetTime() is wall-clock (the kernel
    // keeps the tick->ms coefficients correct across clock changes), so the step rate
    // stays at 60 Hz on both Old and New 3DS. ms -> CLOCK_HZ (32768) ticks; the low 16
    // bits suffice (the loop uses u16 deltas and tolerates the ~2 s wrap).
    return (uint16_t)((osGetTime() * CLOCK_HZ) / 1000);
}

void platform_vsync(void) {
    // Push the frame drawn since the last vsync to the LCD, then wait for the next
    // vertical blank to pace the loop (flush+swap then vblank, the libctru idiom).
    gfx3ds_finalize();
    gspWaitForVBlank();
    // Service the applet loop (HOME menu, sleep, power). When the user closes the
    // app, run the atexit cleanup and exit the process cleanly.
    if (!aptMainLoop()) exit(0);
}

// --- Stereoscopic 3D helpers (declared in platform.h) ----------------------

int stereo_active(void) {
    float s = osGet3DSliderState();   // 0.0 (off) .. 1.0 (max separation)
    g_slider_q8 = (int)(s * 256.0f);
    return g_slider_q8 > 0;
}

int stereo_px(int depth) {
    // Parallax in px for this layer, scaled by the current slider position.
    return (depth * g_slider_q8) >> 8;
}
