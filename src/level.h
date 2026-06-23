#ifndef LEVEL_H
#define LEVEL_H

#include "platform.h"

// Name of the embedded levels/*.mrg mod, baked in by the Makefile per ROM.
// Shown on the league screen under the title. Defaults empty for standalone builds.
#ifndef MOD_NAME
#define MOD_NAME ""
#endif

// Symbols from levels.o
extern const uint8_t _binary_levels_mrg_start[];
extern const uint8_t _binary_levels_mrg_end[];

int convert_coord(int32_t raw);
int get_pixel_coord(int32_t internal);
const uint8_t* get_track_data(const uint8_t* mrg, int level_idx, int track_idx);
// Number of tracks in a league.
int level_track_count(const uint8_t* mrg, int level_idx);
// Flat global track index: tracks of all leagues before `league`, plus `track`.
// This is the key into the SaveData arrays (see save.h).
int global_track_index(const uint8_t* mrg, int league, int track);
// A league is unlocked if it is the first or the previous league is fully done.
int league_unlocked(const uint8_t* mrg, int league);
// A track is unlocked if its league is and the previous track is completed.
int track_unlocked(const uint8_t* mrg, int league, int track);
void get_track_flags(const uint8_t* data, int* start_x, int* start_y, int* finish_x, int* finish_y);
void draw_track(const uint8_t* data, int cam_x, int cam_y);
// Draw the start/finish flags. Call AFTER draw_bike so flags the rider passes
// stay on top of the moto rather than being hidden behind it.
void draw_track_flags(const uint8_t* data, int cam_x, int cam_y);
// Aspect-preserving fit of the whole track into a box, returned so a caller can
// place its own markers (e.g. the gameplay minimap) without re-tracing.
typedef struct { int min_x, min_y, off_x, off_y, draw_h, scale; } TrackPreviewXform;
// Draw a scaled-to-fit silhouette of the whole track inside the box (bx,by,bw,bh)
// in `track_color`, plus start (green dot) and finish (checkered square) markers.
// Returns the fit transform in *xf (pass NULL if unused, e.g. for the static menu
// previews; the minimap keeps it to place the live bike dot). Used by the
// track-select screen; does not touch the physics geometry. Returns 0 if the
// track data is unusable.
int draw_track_preview_flags(const uint8_t* data, int bx, int by, int bw, int bh,
                             color_t track_color, TrackPreviewXform* xf);
// Map a world point in pixel coords into the box (use the xf from above).
void track_preview_map(const TrackPreviewXform* xf, int px, int py, int* sx, int* sy);
// Name (NUL-terminated) of a track within a league, for menu detail panes.
const char* get_track_name(const uint8_t* mrg, int league, int track);
// Project an internal track point (ix,iy) to the centre of the 3D ribbon on
// screen (midway between the near riding edge and the far ribbon edge), using the
// same camera offset (ox,oy) as draw_track. Used to lay the bike shadow on the
// ribbon surface.
void project_track_center(int32_t ix, int32_t iy, int ox, int oy, int* cx, int* cy);

#endif // LEVEL_H
