#include <3ds.h>
#include <string.h>
#include "graphics.h"

// Nintendo 3DS display backend. Unlike the GBA/NDS, where the back buffer matches
// the framebuffer and present is a straight copy, the 3DS framebuffer is (a) in
// 24-bit BGR8 (the gfxInitDefault default — 3 bytes/pixel, byte order B,G,R), not
// the game's 15-bit BGR, and (b) stored rotated 90 degrees (memory columns run
// along the screen's X, and each column runs bottom-to-top). So present walks the
// back buffer and scatters each pixel to the byte offset for (x, y), converting
// the colour as it goes. The loop runs column-major (x outer, y inner) so the
// framebuffer writes step down one column contiguously and stay cache-friendly.
//
// The game renders at 400x240, the top screen's native size, so the top present
// fills it 1:1. The bottom screen is only 320 wide, so its present copies the
// centred 320px window of the (400-wide) sub back buffer — every dual-screen
// layout is centred on SCREEN_WIDTH/2, so nothing important falls in the margins.
//
// Each frame ends with gfx3ds_finalize() (gfxFlushBuffers + gfxSwapBuffers, the
// idiomatic libctru present), driven from platform_vsync(); see platform_3ds.c.

#define FB_H    240                           // framebuffer "height" = screen height
#define BPP     3                             // GSP_BGR8_OES: 3 bytes/pixel
#define BOT_W   320                           // bottom LCD width (px)
#define SUB_X   ((SCREEN_WIDTH - BOT_W) / 2)  // left edge of the centred bottom window

// Present walks the rotated framebuffer column by column, but g_backbuf is
// row-major, so a straight column read strides a whole row (SCREEN_WIDTH*2 bytes)
// and misses the cache on every pixel — the present was the frame-rate bottleneck.
// Walking the copy in TILE-wide tiles instead lets the 16 columns of a tile reuse
// the cache lines already pulled in (one 32-byte line spans 16 px), cutting read
// misses ~16x. 16 divides the top width (400), bottom width (320) and height (240).
#define TILE    16

// Current fade level, 0 = normal .. 16 = black, applied during colour conversion
// to mimic the GBA/NDS hardware brightness fades (the 3DS has no equivalent
// per-frame brightness register, so we dim in software here instead).
static int g_fade;

// Expand a 5-bit channel to 8-bit (x -> x*255/31) and apply the fade.
static inline u8 chan8(int c5, int fade) {
    int c8 = (c5 << 3) | (c5 >> 2);
    if (fade) c8 = (c8 * (16 - fade)) >> 4;
    return (u8)c8;
}

// Write one 15-bit BGR555 game pixel to a BGR8 framebuffer slot (bytes B,G,R).
static inline void put888(u8* p, color_t c, int fade) {
    p[0] = chan8((c >> 10) & 0x1F, fade);  // B
    p[1] = chan8((c >> 5) & 0x1F, fade);   // G
    p[2] = chan8(c & 0x1F, fade);          // R
}

void clear_screen(color_t color) {
    // Two pixels per store: the back buffer is 4-byte aligned and 96000 px (even),
    // so fill it as packed 32-bit words. Runs once per eye on the stereo path.
    uint32_t c2 = (uint32_t)color | ((uint32_t)color << 16);
    uint32_t* b = (uint32_t*)g_backbuf;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT / 2; i++) b[i] = c2;
}

// Present g_backbuf to one eye of the top screen (eye 0 = left, 1 = right). The
// stereo gameplay path renders a differently-parallaxed frame into g_backbuf
// before each call; menus call present_frame() which presents the same buffer to
// both eyes (a flat, comfortable 2D image). Writes only — the frame is pushed to
// the LCD by gfx3ds_finalize() at the next vsync.
void present_frame_top(int eye) {
    u8* fb = gfxGetFramebuffer(GFX_TOP, eye ? GFX_RIGHT : GFX_LEFT, NULL, NULL);
    int fade = g_fade;
    for (int x0 = 0; x0 < SCREEN_WIDTH; x0 += TILE) {     // 400 cols, fills the top 1:1
        for (int y0 = 0; y0 < SCREEN_HEIGHT; y0 += TILE) {
            for (int x = x0; x < x0 + TILE; x++) {
                u8* col = fb + (size_t)x * FB_H * BPP;    // this screen column
                const color_t* src = g_backbuf + y0 * SCREEN_WIDTH + x;
                for (int y = y0; y < y0 + TILE; y++) {
                    put888(col + (FB_H - 1 - y) * BPP, *src, fade);
                    src += SCREEN_WIDTH;
                }
            }
        }
    }
}

void present_frame(void) {
    // Mono: copy the back buffer to both eyes so the image is flat regardless of
    // the 3D slider position (used for menus and the stereo-disabled gameplay path).
    present_frame_top(0);
    present_frame_top(1);
}

// Bottom (sub) screen present: the bottom LCD is 320 wide, narrower than the
// 400-wide sub back buffer, so copy the centred 320px window (columns SUB_X..).
void present_sub_frame(void) {
    u8* fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    for (int sx0 = 0; sx0 < BOT_W; sx0 += TILE) {        // 320 bottom-screen columns
        for (int y0 = 0; y0 < SCREEN_HEIGHT; y0 += TILE) {
            for (int sx = sx0; sx < sx0 + TILE; sx++) {
                u8* col = fb + (size_t)sx * FB_H * BPP;
                // centred window of the sub buffer
                const color_t* src = g_subbuf + y0 * SCREEN_WIDTH + (sx + SUB_X);
                for (int y = y0; y < y0 + TILE; y++) {
                    put888(col + (FB_H - 1 - y) * BPP, *src, 0);  // bottom never fades
                    src += SCREEN_WIDTH;
                }
            }
        }
    }
}

// Push the framebuffers to the LCD (the idiomatic libctru present). Called once
// per frame from platform_vsync after all screen writes; also used by the fades.
void gfx3ds_finalize(void) {
    gfxFlushBuffers();
    gfxSwapBuffers();
}

// Fill every framebuffer black once at init (see platform_init), so nothing
// flashes before the first present writes every pixel.
void gfx3ds_clear_black(void) {
    u16 w, h;
    u8* fb;
    fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h);
    memset(fb, 0, (size_t)w * h * BPP);
    fb = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, &w, &h);
    memset(fb, 0, (size_t)w * h * BPP);
    fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    memset(fb, 0, (size_t)w * h * BPP);
    gfx3ds_finalize();
}

// Software brightness fades of the top screen, mirroring the GBA BLDY darken /
// NDS master-brightness transitions. Each step re-presents whatever is currently
// in g_backbuf at the new fade level and pushes a frame, so the displayed image
// dims to black (fade_out) or rises from black (fade_in). The bottom screen is
// left at full brightness, matching the NDS backend.
#define FADE_STEP 3

void fade_out(void) {
    for (int f = 0; f <= 16; f += FADE_STEP) {
        g_fade = f; present_frame(); gfx3ds_finalize(); gspWaitForVBlank();
    }
    g_fade = 16; present_frame(); gfx3ds_finalize();
}

void fade_in(void) {
    for (int f = 16; f > 0; f -= FADE_STEP) {
        g_fade = f; present_frame(); gfx3ds_finalize(); gspWaitForVBlank();
    }
    g_fade = 0; present_frame(); gfx3ds_finalize(); gspWaitForVBlank();
}
