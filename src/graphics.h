#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "gba.h"

void clear_screen(color_t color);
// Blit the back buffer to VRAM (call once per frame at VBlank start).
void present_frame(void);
void put_pixel(int x, int y, color_t color);
void draw_rect(int x, int y, int w, int h, color_t color);
void draw_char(int x, int y, char c, color_t color);
void draw_string(int x, int y, const char* str, color_t color);
void draw_line(int x1, int y1, int x2, int y2, color_t color);
// Blit a sprite: each entry's bit15 set = opaque pixel (low 15 bits = color).
void draw_sprite(int x, int y, const color_t* data, int w, int h);
// Blit one frame from a 6-column rotation sheet (col = frame%6, row = frame/6).
void draw_sprite_frame(int x, int y, const color_t* sheet, int sheet_w,
                       int fw, int fh, int frame);
void fill_circle(int cx, int cy, int r, color_t color);

#endif // GRAPHICS_H
