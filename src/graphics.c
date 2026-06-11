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
};

void clear_screen(color_t color) {
    uint32_t* vram32 = (uint32_t*)VRAM;
    uint32_t c32 = color | (color << 16);
    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT) / 2; i++) {
        vram32[i] = c32;
    }
}

void put_pixel(int x, int y, color_t color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        ((color_t*)VRAM)[y * SCREEN_WIDTH + x] = color;
    }
}

void draw_rect(int x, int y, int w, int h, color_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    color_t* vram = (color_t*)VRAM;
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

void draw_char(int x, int y, char c, color_t color) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 32 || c > 90) return;
    int idx = (c - 32) * 5;
    for (int col = 0; col < 5; col++) {
        uint8_t bits = font5x7[idx + col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_string(int x, int y, const char* str, color_t color) {
    while (*str) {
        draw_char(x, y, *str++, color);
        x += 6;
    }
}

void draw_sprite(int x, int y, const color_t* data, int w, int h) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            color_t c = data[row * w + col];
            if (c & 0x8000) put_pixel(x + col, y + row, c & 0x7FFF);
        }
    }
}

void fill_circle(int cx, int cy, int r, color_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) put_pixel(cx + dx, cy + dy, color);
        }
    }
}

void draw_line(int x1, int y1, int x2, int y2, color_t color) {
    // Fast path for vertical lines
    if (x1 == x2) {
        if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
        if (y1 < 0) y1 = 0;
        if (y2 >= SCREEN_HEIGHT) y2 = SCREEN_HEIGHT - 1;
        for (int y = y1; y <= y2; y++) {
            ((color_t*)VRAM)[y * SCREEN_WIDTH + x1] = color;
        }
        return;
    }

    // Fast path for horizontal lines
    if (y1 == y2) {
        if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
        if (x1 < 0) x1 = 0;
        if (x2 >= SCREEN_WIDTH) x2 = SCREEN_WIDTH - 1;
        
        int w = x2 - x1 + 1;
        if (w <= 0) return;
        
        color_t* line = &((color_t*)VRAM)[y1 * SCREEN_WIDTH + x1];
        uint32_t c32 = color | (color << 16);
        int i = 0;
        if (((uintptr_t)line & 2) && i < w) {
            line[i++] = color;
        }
        uint32_t* line32 = (uint32_t*)&line[i];
        int w32 = (w - i) / 2;
        for (int k = 0; k < w32; k++) {
            line32[k] = c32;
        }
        i += w32 * 2;
        if (i < w) {
            line[i] = color;
        }
        return;
    }

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x1 >= 0 && x1 < SCREEN_WIDTH && y1 >= 0 && y1 < SCREEN_HEIGHT) {
            ((color_t*)VRAM)[y1 * SCREEN_WIDTH + x1] = color;
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}
