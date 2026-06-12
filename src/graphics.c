#include "graphics.h"
#include <stdlib.h>

const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // Space
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x14, 0x08, 0x3E, 0x08, 0x14, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x08, 0x14, 0x22, 0x41, 0x00, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x00, 0x41, 0x22, 0x14, 0x08, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x09, 0x01, // F
    0x3E, 0x41, 0x49, 0x49, 0x7A, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x3F, 0x40, 0x38, 0x40, 0x3F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x07, 0x08, 0x70, 0x08, 0x07, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x7F, 0x41, 0x41, 0x00, // [
    0x02, 0x04, 0x08, 0x10, 0x20, // backslash
    0x00, 0x41, 0x41, 0x7F, 0x00, // ]
    0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x40, 0x40, 0x40, 0x40, 0x40, // _  (underscore, for the repo URL)
};

// Source word for DMA fill — must live in RAM (IWRAM via .bss).
static uint32_t dma_fill_word;

// Off-screen back buffer: all drawing targets this, and present_frame() blits
// it to VRAM during VBlank. Single-buffered MODE3 has no page flip, so without
// this the renderer races the beam and the last things drawn (the rider) tear.
// On the host harness the "VRAM" is just g_vram_buf, so draw straight into it.
#ifdef HOST_BUILD
extern uint16_t g_vram_buf[];
#define G_CANVAS ((color_t*)g_vram_buf)
#else
static color_t g_backbuf[SCREEN_WIDTH * SCREEN_HEIGHT] __attribute__((section(".ewram")));
#define G_CANVAS g_backbuf
#endif

void clear_screen(color_t color) {
    dma_fill_word = color | ((uint32_t)color << 16);
    REG_DMA3SAD   = (uint32_t)&dma_fill_word;
    REG_DMA3DAD   = (uint32_t)G_CANVAS;
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
        REG_DMA3SAD   = (uint32_t)(G_CANVAS + off);
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

void put_pixel(int x, int y, color_t color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        G_CANVAS[y * SCREEN_WIDTH + x] = color;
    }
}

IWRAM_FN void draw_rect(int x, int y, int w, int h, color_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    color_t* vram = G_CANVAS;
    uint32_t c32 = color | (color << 16);

    for (int j = 0; j < h; j++) {
        color_t* line = &vram[(y + j) * SCREEN_WIDTH + x];
        int i = 0;
        // Align to 32-bit if necessary
        if (((uintptr_t)line & 2) && i < w) {
            line[i++] = color;
        }
        // Fill 32-bit chunks
        uint32_t* line32 = (uint32_t*)&line[i];
        int w32 = (w - i) / 2;
        for (int k = 0; k < w32; k++) {
            line32[k] = c32;
        }
        i += w32 * 2;
        // Remaining pixel
        if (i < w) {
            line[i] = color;
        }
    }
}

// Direct canvas writes (no per-pixel put_pixel call) so per-frame menu text is
// cheap; the unsigned compares fold each clip test into one branch.
IWRAM_FN void draw_char(int x, int y, char c, color_t color) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 32 || c > 95) return;
    const uint8_t* g = &font5x7[(c - 32) * 5];
    color_t* canvas = G_CANVAS;
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        int px = x + col;
        if ((unsigned)px >= SCREEN_WIDTH) continue;
        for (int row = 0; bits; row++, bits >>= 1) {
            if (bits & 1) {
                int py = y + row;
                if ((unsigned)py < SCREEN_HEIGHT)
                    canvas[py * SCREEN_WIDTH + px] = color;
            }
        }
    }
}

IWRAM_FN void draw_string(int x, int y, const char* str, color_t color) {
    while (*str) {
        draw_char(x, y, *str++, color);
        x += 6;
    }
}

// Each set font pixel becomes a scale x scale block written inline (no draw_rect
// call per pixel — at 2x that was ~1400 calls/frame for the menu title).
IWRAM_FN void draw_string_scaled(int x, int y, const char* str, color_t color, int scale) {
    color_t* canvas = G_CANVAS;
    while (*str) {
        char c = *str++;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 32 && c <= 95) {
            const uint8_t* g = &font5x7[(c - 32) * 5];
            for (int col = 0; col < 5; col++) {
                uint8_t bits = g[col];
                int bx = x + col * scale;
                for (int row = 0; bits; row++, bits >>= 1) {
                    if (!(bits & 1)) continue;
                    int by = y + row * scale;
                    for (int dy = 0; dy < scale; dy++) {
                        int py = by + dy;
                        if ((unsigned)py >= SCREEN_HEIGHT) continue;
                        color_t* p = canvas + py * SCREEN_WIDTH + bx;
                        for (int dx = 0; dx < scale; dx++)
                            if ((unsigned)(bx + dx) < SCREEN_WIDTH) p[dx] = color;
                    }
                }
            }
        }
        x += 6 * scale;
    }
}

void draw_string_scaled_outlined(int x, int y, const char* str, color_t fg, color_t outline, int scale) {
    draw_string_scaled(x - 1, y,     str, outline, scale);
    draw_string_scaled(x + 1, y,     str, outline, scale);
    draw_string_scaled(x,     y - 1, str, outline, scale);
    draw_string_scaled(x,     y + 1, str, outline, scale);
    draw_string_scaled(x,     y,     str, fg,      scale);
}

void draw_string_outlined(int x, int y, const char* str, color_t fg, color_t outline) {
    // 4-neighbour halo, then the glyph on top. Cheap (5 passes of a 5x7 font)
    // and enough to keep text readable over the track without a backing box.
    draw_string(x - 1, y,     str, outline);
    draw_string(x + 1, y,     str, outline);
    draw_string(x,     y - 1, str, outline);
    draw_string(x,     y + 1, str, outline);
    draw_string(x,     y,     str, fg);
}

IWRAM_FN void draw_sprite(int x, int y, const color_t* data, int w, int h) {
    int cx0 = x < 0 ? 0 : x;
    int cy0 = y < 0 ? 0 : y;
    int cx1 = x + w > SCREEN_WIDTH  ? SCREEN_WIDTH  : x + w;
    int cy1 = y + h > SCREEN_HEIGHT ? SCREEN_HEIGHT : y + h;
    color_t* vram = G_CANVAS;
    for (int row = cy0; row < cy1; row++) {
        const color_t* src = data + (row - y) * w + (cx0 - x);
        color_t*       dst = vram + row * SCREEN_WIDTH + cx0;
        for (int col = cx0; col < cx1; col++) {
            color_t c = *src++;
            if (c & 0x8000) *dst = c & 0x7FFF;
            dst++;
        }
    }
}

IWRAM_FN void draw_sprite_frame(int x, int y, const color_t* sheet, int sheet_w,
                                int fw, int fh, int frame) {
    int sx = (frame % 6) * fw;
    int sy = (frame / 6) * fh;
    int cx0 = x < 0 ? 0 : x;
    int cy0 = y < 0 ? 0 : y;
    int cx1 = x + fw > SCREEN_WIDTH  ? SCREEN_WIDTH  : x + fw;
    int cy1 = y + fh > SCREEN_HEIGHT ? SCREEN_HEIGHT : y + fh;
    color_t* vram = G_CANVAS;
    for (int row = cy0; row < cy1; row++) {
        const color_t* src = sheet + (sy + row - y) * sheet_w + sx + (cx0 - x);
        color_t*       dst = vram + row * SCREEN_WIDTH + cx0;
        for (int col = cx0; col < cx1; col++) {
            color_t c = *src++;
            if (c & 0x8000) *dst = c & 0x7FFF;
            dst++;
        }
    }
}

IWRAM_FN void fill_circle(int cx, int cy, int r, color_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) put_pixel(cx + dx, cy + dy, color);
        }
    }
}

IWRAM_FN void draw_line(int x1, int y1, int x2, int y2, color_t color) {
    color_t* fb = G_CANVAS;

    // Fast path for vertical lines
    if (x1 == x2) {
        if ((unsigned)x1 >= (unsigned)SCREEN_WIDTH) return;
        if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
        if (y1 < 0) y1 = 0;
        if (y2 >= SCREEN_HEIGHT) y2 = SCREEN_HEIGHT - 1;
        color_t* p = fb + y1 * SCREEN_WIDTH + x1;
        for (int y = y1; y <= y2; y++, p += SCREEN_WIDTH)
            *p = color;
        return;
    }

    // Fast path for horizontal lines (32-bit word writes)
    if (y1 == y2) {
        if ((unsigned)y1 >= (unsigned)SCREEN_HEIGHT) return;
        if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
        if (x1 < 0) x1 = 0;
        if (x2 >= SCREEN_WIDTH) x2 = SCREEN_WIDTH - 1;
        int w = x2 - x1 + 1;
        if (w <= 0) return;
        color_t* line = fb + y1 * SCREEN_WIDTH + x1;
        uint32_t c32 = color | ((uint32_t)color << 16);
        int i = 0;
        if (((uintptr_t)line & 2) && i < w) line[i++] = color;
        uint32_t* line32 = (uint32_t*)&line[i];
        int w32 = (w - i) / 2;
        for (int k = 0; k < w32; k++) line32[k] = c32;
        i += w32 * 2;
        if (i < w) line[i] = color;
        return;
    }

    // Bresenham: incremental pointer avoids per-pixel multiply,
    // unsigned cast folds the 0<=x<W and 0<=y<H checks into one compare each.
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    int step_y = sy * SCREEN_WIDTH;
    color_t* p = fb + y1 * SCREEN_WIDTH + x1;

    while (1) {
        if ((unsigned)x1 < (unsigned)SCREEN_WIDTH && (unsigned)y1 < (unsigned)SCREEN_HEIGHT)
            *p = color;
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; p += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; p += step_y; }
    }
}
