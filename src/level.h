#ifndef LEVEL_H
#define LEVEL_H

#include "gba.h"

// Symbols from levels.o
extern const uint8_t _binary_levels_mrg_start[];
extern const uint8_t _binary_levels_mrg_end[];

int convert_coord(int32_t raw);
int get_pixel_coord(int32_t internal);
const uint8_t* get_track_data(const uint8_t* mrg, int level_idx, int track_idx);
// Number of tracks in a league.
int level_track_count(const uint8_t* mrg, int level_idx);
void get_track_flags(const uint8_t* data, int* start_x, int* start_y, int* finish_x, int* finish_y);
void draw_track(const uint8_t* data, int cam_x, int cam_y);
// Draw a scaled-to-fit silhouette of the whole track inside the box (bx,by,bw,bh)
// in `color`. Used by the track-select screen; does not touch the physics geometry.
void draw_track_preview(const uint8_t* data, int bx, int by, int bw, int bh, color_t color);

#endif // LEVEL_H
