#include <pspgu.h>
#include <pspdisplay.h>
#include <psputils.h>
#include "graphics.h"

// Sony PSP display backend. The game renders at the native 480x272 into the
// shared back buffer (g_backbuf, in main RAM); present_frame() hands it to the
// Graphics Engine (GU) as a texture and draws it across the screen, filling the
// LCD 1:1.
//
// Why the GU rather than poking VRAM directly: the engine is single-buffered —
// platform_vsync() runs every frame just to pace, while present_frame() only runs
// when something changed, so the displayed buffer must persistently hold the last
// frame (see the main loop). A pure CPU-written framebuffer with no GE activity is
// not reliably scanned out by emulators (PPSSPP shows it black), whereas a GU draw
// is. So we keep the single-buffer model — the GU draw and display buffers are the
// same VRAM buffer, and there is no page flip — but route the present through the
// GE. The back buffer is uploaded as a GU_PSM_5551 texture: that format's bit
// layout (R 0-4, G 5-9, B 10-14, A 15) matches the game's COLOR()/BGR555 exactly,
// so no per-pixel conversion is needed; GU_TCC_RGB ignores the unused top bit.
//
// The framebuffer is wider than a single GU texture tile streams comfortably, so
// the blit is drawn in vertical slices (the standard PSP full-screen-texture idiom).

#define BUF_W   512   // VRAM buffer line stride in px (480 rounded up to a pow2)
#define VRAM0   0     // single shared draw/display buffer at EDRAM offset 0
#define SLICE   64    // blit the back buffer in 64px-wide textured strips

// 2D textured-sprite vertex: 16-bit texture coords followed by 16-bit screen
// coords (the order the GU expects for GU_TEXTURE_16BIT | GU_VERTEX_16BIT). With
// GU_TRANSFORM_2D the x/y are raw screen pixels, so no projection setup is needed.
struct TVertex { unsigned short u, v; short x, y, z; };
// Flat-colour 2D vertex for the fade overlay. The 32-bit colour must stay 4-byte
// aligned, so the struct is padded to a 12-byte stride (the GE's stride for
// GU_COLOR_8888 | GU_VERTEX_16BIT), matching how the GE walks the vertex array.
struct CVertex { unsigned int color; short x, y, z; short pad; };

// GU display list. 64 KB is ample for one frame (a texture state change plus a
// handful of slice sprites and an optional fade quad).
static unsigned int gu_list[16384] __attribute__((aligned(16)));

// Current fade level, 0 = normal .. 16 = black. Drawn as a translucent black
// overlay after the framebuffer blit, mimicking the GBA BLDY / NDS master-
// brightness hardware fades (the GE has no per-frame brightness register).
static int g_fade;

void gfx_psp_init(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);

    // Single-buffered: the draw buffer and the display buffer are the same VRAM
    // buffer (offset 0), so a present is immediately on screen with no page flip —
    // exactly the model the GBA/NDS/3DS backends use. Buffer pointers are
    // EDRAM-relative offsets; the GU adds the VRAM base itself.
    sceGuDrawBuffer(GU_PSM_5551, (void*)VRAM0, BUF_W);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void*)VRAM0, BUF_W);

    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    // Clear the buffer to black once so nothing flashes before the first present.
    sceGuClearColor(0);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void clear_screen(color_t color) {
    color_t* b = g_backbuf;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) b[i] = color;
}

void present_frame(void) {
    // The back buffer was just filled by the CPU rasteriser; flush it from the
    // data cache so the GE reads the current pixels (not stale cached lines).
    sceKernelDcacheWritebackRange(g_backbuf, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(color_t));

    sceGuStart(GU_DIRECT, gu_list);

    // Draw the back buffer as a full-screen texture. tbw = SCREEN_WIDTH (480) so
    // the GE reads the 480-stride buffer correctly; the 512x512 size only governs
    // wrapping, which CLAMP + exact UVs make moot. REPLACE/RGB copies pixels 1:1.
    sceGuTexMode(GU_PSM_5551, 0, 0, 0);
    sceGuTexImage(0, 512, 512, SCREEN_WIDTH, g_backbuf);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_TEXTURE_2D);

    for (int x = 0; x < SCREEN_WIDTH; x += SLICE) {
        int w = (x + SLICE > SCREEN_WIDTH) ? (SCREEN_WIDTH - x) : SLICE;
        struct TVertex* v = (struct TVertex*)sceGuGetMemory(2 * sizeof(struct TVertex));
        v[0].u = x;     v[0].v = 0;             v[0].x = x;     v[0].y = 0;             v[0].z = 0;
        v[1].u = x + w; v[1].v = SCREEN_HEIGHT; v[1].x = x + w; v[1].y = SCREEN_HEIGHT; v[1].z = 0;
        sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
    }

    // Fade overlay: a translucent black quad over the whole screen. alpha scales
    // 0..16 -> 0..255, so g_fade == 16 is fully black.
    if (g_fade > 0) {
        sceGuDisable(GU_TEXTURE_2D);
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        int a = g_fade * 16; if (a > 255) a = 255;
        unsigned int col = (unsigned int)a << 24;   // ABGR8888: black, alpha = a
        struct CVertex* q = (struct CVertex*)sceGuGetMemory(2 * sizeof(struct CVertex));
        q[0].color = col; q[0].x = 0;            q[0].y = 0;             q[0].z = 0; q[0].pad = 0;
        q[1].color = col; q[1].x = SCREEN_WIDTH; q[1].y = SCREEN_HEIGHT; q[1].z = 0; q[1].pad = 0;
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, q);
        sceGuDisable(GU_BLEND);
    }

    sceGuFinish();
    sceGuSync(0, 0);
}

// Software brightness fades, mirroring the GBA BLDY darken / NDS master-brightness
// transitions and the 3DS software fades. Each step re-presents g_backbuf with a
// heavier black overlay and waits a VBlank, so the image dims to black (fade_out)
// or rises from black (fade_in).
#define FADE_STEP 3

void fade_out(void) {
    for (int f = 0; f <= 16; f += FADE_STEP) {
        g_fade = f; present_frame(); sceDisplayWaitVblankStart();
    }
    g_fade = 16; present_frame();
}

void fade_in(void) {
    for (int f = 16; f > 0; f -= FADE_STEP) {
        g_fade = f; present_frame(); sceDisplayWaitVblankStart();
    }
    g_fade = 0; present_frame(); sceDisplayWaitVblankStart();
}
