#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "gba.h"

void clear_screen(color_t color);
void put_pixel(int x, int y, color_t color);
void draw_rect(int x, int y, int w, int h, color_t color);
void draw_char(int x, int y, char c, color_t color);
void draw_string(int x, int y, const char* str, color_t color);
void draw_line(int x1, int y1, int x2, int y2, color_t color);
// Blit a sprite: each entry's bit15 set = opaque pixel (low 15 bits = color).
void draw_sprite(int x, int y, const color_t* data, int w, int h);
void fill_circle(int cx, int cy, int r, color_t color);

#endif // GRAPHICS_H
