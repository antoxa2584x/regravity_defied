#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include "platform.h"

// Sony PlayStation Portable implementation of the platform interface (see
// platform.h). The game's software rasteriser draws into the shared back buffer
// (g_backbuf); the display backend (graphics_psp.c) copies it to the PSP's VRAM
// framebuffer each frame. The PSP has a single 480x272 screen, so it renders at
// that native size (SCREEN_WIDTH/HEIGHT) and reuses the GBA's single-screen
// layout — it is not a DUAL_SCREEN target.
//
// This file also carries the PSP module header. PSP_MODULE_INFO / the main
// thread attributes define special ELF sections the kernel reads when loading
// the EBOOT; they may live in any linked object, so they sit beside the rest of
// the PSP-specific bring-up here rather than in the portable main.c.
PSP_MODULE_INFO("ReGravityDefied", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

// Implemented in graphics_psp.c: gfx_psp_init() brings up the Graphics Engine (GU)
// and clears the screen black once, so nothing flashes before the first present.
void gfx_psp_init(void);

// --- HOME / exit handling --------------------------------------------------
// Standard PSP exit-callback boilerplate: the kernel signals the registered
// callback when the user picks "Exit Game" from the HOME menu; we then ask the
// kernel to tear the game down cleanly (sceKernelExitGame), matching how the DS
// build returns to the launcher and the 3DS build honours aptMainLoop().

static int exit_callback(int arg1, int arg2, void* common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void* argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();   // wait for callbacks for the life of the thread
    return 0;
}

static void setup_exit_callback(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread,
                                     0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

void platform_init(void) {
    setup_exit_callback();

    // Bring up the display (Graphics Engine + 480x272 buffer), cleared once
    // (see graphics_psp.c).
    gfx_psp_init();

    // Buttons: poll the digital pad plus the analog stick (PSP_CTRL_MODE_ANALOG
    // is what makes Lx/Ly readable). Sampling cycle 0 = read on demand.
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

uint16_t platform_keys(void) {
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);
    unsigned int b = pad.Buttons;

    // Map PSP buttons onto the game's logical KEY_* layout (GBA bit order). Cross
    // accelerates (A), Circle/Square brake (B) — Square is offered as an alias so
    // either face button on the right brakes. Triggers map to L/R; the D-pad and
    // Start/Select pass straight through.
    uint16_t g = 0;
    if (b & PSP_CTRL_CROSS)                       g |= KEY_A;
    if (b & (PSP_CTRL_CIRCLE | PSP_CTRL_SQUARE))  g |= KEY_B;
    if (b & PSP_CTRL_SELECT)                      g |= KEY_SELECT;
    if (b & PSP_CTRL_START)                       g |= KEY_START;
    if (b & PSP_CTRL_RIGHT)                       g |= KEY_RIGHT;
    if (b & PSP_CTRL_LEFT)                        g |= KEY_LEFT;
    if (b & PSP_CTRL_UP)                          g |= KEY_UP;
    if (b & PSP_CTRL_DOWN)                        g |= KEY_DOWN;
    if (b & PSP_CTRL_RTRIGGER)                    g |= KEY_R;
    if (b & PSP_CTRL_LTRIGGER)                    g |= KEY_L;

    // Map the analog stick onto the D-pad so it can steer/lean too, mirroring the
    // 3DS Circle Pad mapping. Lx/Ly are 0..255, centred at 128; a generous dead
    // zone (~half deflection) keeps a resting stick from registering as held.
    if (pad.Lx < 80)  g |= KEY_LEFT;
    if (pad.Lx > 176) g |= KEY_RIGHT;
    if (pad.Ly < 80)  g |= KEY_UP;
    if (pad.Ly > 176) g |= KEY_DOWN;

    return g;
}

uint16_t platform_timer(void) {
    // Game clock from the microsecond system clock, scaled to CLOCK_HZ (32768).
    // The fixed-timestep loop (main.c) uses 16-bit deltas and tolerates the ~2 s
    // wrap, so the low 16 bits suffice. The 64-bit multiply can't overflow for any
    // realistic uptime.
    uint64_t us = sceKernelGetSystemTimeWide();
    return (uint16_t)((us * CLOCK_HZ) / 1000000ULL);
}

void platform_vsync(void) {
    sceDisplayWaitVblankStart();
}
