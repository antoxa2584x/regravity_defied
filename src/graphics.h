#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "platform.h"

// Off-screen frame back buffer (the drawing canvas). All rasteriser primitives
// in graphics.c target this; the per-platform backend (graphics_gba.c /
// graphics_nds.c) presents it to the display. Not present in the host harness,
// where drawing goes straight into the test buffer.
#ifndef HOST_BUILD
extern color_t g_backbuf[SCREEN_WIDTH * SCREEN_HEIGHT];
#endif

// --- Display backend (implemented per target) ------------------------------
void clear_screen(color_t color);
// Blit the back buffer to VRAM (call once per frame at VBlank start).
void present_frame(void);
// Blocking hardware brightness fades of the displayed BG2 bitmap (each spins a
// few VBlanks). fade_out darkens whatever is currently in VRAM to black;
// fade_in brings it back and disables the blend. Used for menu transitions.
void fade_out(void);
void fade_in(void);
void put_pixel(int x, int y, color_t color);
void draw_rect(int x, int y, int w, int h, color_t color);
void draw_char(int x, int y, char c, color_t color);
void draw_string(int x, int y, const char* str, color_t color);
// draw_string with a 1px outline in `outline` behind `fg`, so HUD text stays
// legible over any background (track lines, rider) without a solid box.
void draw_string_outlined(int x, int y, const char* str, color_t fg, color_t outline);
// draw_string with each font pixel drawn as a scale x scale block (scale>=1).
// Glyph advance is 6*scale px.
void draw_string_scaled(int x, int y, const char* str, color_t color, int scale);
// Scaled string with a 1px outline in `outline` behind `fg`.
void draw_string_scaled_outlined(int x, int y, const char* str, color_t fg, color_t outline, int scale);
void draw_line(int x1, int y1, int x2, int y2, color_t color);
// Blit a sprite: each entry's bit15 set = opaque pixel (low 15 bits = color).
void draw_sprite(int x, int y, const color_t* data, int w, int h);
// Blit one frame from a 6-column rotation sheet (col = frame%6, row = frame/6).
void draw_sprite_frame(int x, int y, const color_t* sheet, int sheet_w,
                       int fw, int fh, int frame);
void fill_circle(int cx, int cy, int r, color_t color);

#endif // GRAPHICS_H
