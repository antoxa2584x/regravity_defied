#include "level.h"
#include "graphics.h"
#include "gd_sprites.h"
#include "physics.h"
#include "save.h"

static int s_flag_tick = 0;

int convert_coord(int32_t raw) {
    // raw comes from read_be32, so it's a 32-bit signed integer.
    // In reference: addPointSimple(var << 16 >> 3)
    // To get pixel: point << 3 >> 16
    return (int)((int32_t)raw << 16) >> 3;
}

int get_pixel_coord(int32_t internal) {
    return (int)((int32_t)internal << 3) >> 16;
}

int level_track_count(const uint8_t* mrg, int level_idx) {
    const uint8_t* p = mrg;
    for (int l = 0; l < level_idx; l++) {
        uint32_t count = read_be32(p);
        p += 4;
        for (int t = 0; t < (int)count; t++) {
            p += 4;
            while (*p++) ;
        }
    }
    return (int)read_be32(p);
}

int global_track_index(const uint8_t* mrg, int league, int track) {
    int idx = 0;
    for (int l = 0; l < league; l++) idx += level_track_count(mrg, l);
    return idx + track;
}

int league_unlocked(const uint8_t* mrg, int league) {
    if (league <= 0) return 1;
    int prev_count = level_track_count(mrg, league - 1);
    int base = global_track_index(mrg, league - 1, 0);
    for (int t = 0; t < prev_count; t++)
        if (!save_completed(base + t)) return 0;
    return 1;
}

int track_unlocked(const uint8_t* mrg, int league, int track) {
    if (!league_unlocked(mrg, league)) return 0;
    if (track == 0) return 1;
    return save_completed(global_track_index(mrg, league, track) - 1);
}

const uint8_t* get_track_data(const uint8_t* mrg, int level_idx, int track_idx) {
    const uint8_t* p = mrg;
    for (int l = 0; l < level_idx; l++) {
        uint32_t count = read_be32(p);
        p += 4;
        for (int t = 0; t < (int)count; t++) {
            p += 4;
            while (*p++) ;
        }
    }
    uint32_t count = read_be32(p); (void)count;
    const uint8_t* track_info = p + 4;
    for (int t = 0; t < track_idx; t++) {
        track_info += 4;
        while (*track_info++) ;
    }
    uint32_t track_offset = read_be32(track_info);
    return mrg + track_offset;
}

void get_track_flags(const uint8_t* data, int* start_x, int* start_y, int* finish_x, int* finish_y) {
    const uint8_t* p = data;
    if (*p != 0x33) return;
    p++;

    // These are already in internal F13 format: (value << 16) >> 3
    int32_t start_threshold = (int32_t)read_be32(p); p += 4;
    int32_t start_y_ignored = read_be32(p); p += 4; (void)start_y_ignored;
    int32_t finish_threshold = (int32_t)read_be32(p); p += 4;
    int32_t finish_y_ignored = read_be32(p); p += 4; (void)finish_y_ignored;

    // Fallbacks: if no vertex lies past a threshold, gate on the threshold
    // itself. Normally these are overwritten below with the flag vertex X so the
    // timer triggers exactly where the flag is drawn (the first point past the
    // threshold) — not at the spawn point, which sits on the threshold and would
    // start the clock immediately.
    if (start_x) *start_x = get_pixel_coord(start_threshold);
    if (finish_x) *finish_x = get_pixel_coord(finish_threshold);

    uint16_t points_count = read_be16(p); p += 2;
    int32_t cur_x = convert_coord(read_be32(p)); p += 4;
    int32_t cur_y = convert_coord(read_be32(p)); p += 4;

    int found_start = 0;
    int found_finish = 0;

    for (int i = 0; i < points_count - 1; i++) {
        int8_t dx = (int8_t)*p++;
        if (dx == -1) {
            cur_x = convert_coord(read_be32(p)); p += 4;
            cur_y = convert_coord(read_be32(p)); p += 4;
            continue;
        } else {
            int8_t dy = (int8_t)*p++;
            cur_x += convert_coord(dx);
            cur_y += convert_coord(dy);
        }

        if (!found_start && cur_x > start_threshold) {
            if (start_x) *start_x = get_pixel_coord(cur_x);
            if (start_y) *start_y = get_pixel_coord(cur_y);
            found_start = 1;
        }
        if (!found_finish && cur_x > finish_threshold) {
            if (finish_x) *finish_x = get_pixel_coord(cur_x);
            if (finish_y) *finish_y = get_pixel_coord(cur_y);
            found_finish = 1;
        }
    }
}

// Walk the delta-encoded point stream, advancing (*cx,*cy) one point. dx==-1 is
// an absolute reset (4-byte x, 4-byte y); otherwise a signed (dx,dy) step.
// Returns the cursor advanced past the point. Mirrors the decode in
// get_track_flags so the preview reads the same geometry the game does.
static const uint8_t* next_track_point(const uint8_t* p, int32_t* cx, int32_t* cy) {
    int8_t dx = (int8_t)*p++;
    if (dx == -1) {
        *cx = convert_coord(read_be32(p)); p += 4;
        *cy = convert_coord(read_be32(p)); p += 4;
    } else {
        int8_t dy = (int8_t)*p++;
        *cx += convert_coord(dx);
        *cy += convert_coord(dy);
    }
    return p;
}

void draw_track_preview(const uint8_t* data, int bx, int by, int bw, int bh, color_t color) {
    if (*data != 0x33) return;
    const uint8_t* hdr = data + 1 + 16;          // skip header byte + 4 thresholds
    uint16_t points_count = read_be16(hdr);
    if (points_count < 2) return;
    const uint8_t* first = hdr + 2;

    // Pass 1: pixel-space bounding box of every point.
    int32_t cx = convert_coord(read_be32(first));
    int32_t cy = convert_coord(read_be32(first + 4));
    const uint8_t* p = first + 8;
    int min_x, max_x, min_y, max_y;
    min_x = max_x = get_pixel_coord(cx);
    min_y = max_y = get_pixel_coord(cy);
    for (int i = 0; i < points_count - 1; i++) {
        p = next_track_point(p, &cx, &cy);
        int sx = get_pixel_coord(cx), sy = get_pixel_coord(cy);
        if (sx < min_x) min_x = sx; else if (sx > max_x) max_x = sx;
        if (sy < min_y) min_y = sy; else if (sy > max_y) max_y = sy;
    }
    int span_x = max_x - min_x; if (span_x < 1) span_x = 1;
    int span_y = max_y - min_y; if (span_y < 1) span_y = 1;

    // Aspect-preserving fit: pick the tighter axis (16.16 fixed-point scale).
    int sxf = ((bw - 2) << 16) / span_x;
    int syf = ((bh - 2) << 16) / span_y;
    int scale = sxf < syf ? sxf : syf;
    int draw_w = (int)(((int64_t)span_x * scale) >> 16);
    int draw_h = (int)(((int64_t)span_y * scale) >> 16);
    int off_x = bx + (bw - draw_w) / 2;
    int off_y = by + (bh - draw_h) / 2;

    // Pass 2: project each point into the box (y flipped so higher = up) and
    // connect with line segments.
    cx = convert_coord(read_be32(first));
    cy = convert_coord(read_be32(first + 4));
    p = first + 8;
    int prev_x = off_x + (int)(((int64_t)(get_pixel_coord(cx) - min_x) * scale) >> 16);
    int prev_y = off_y + draw_h - (int)(((int64_t)(get_pixel_coord(cy) - min_y) * scale) >> 16);
    for (int i = 0; i < points_count - 1; i++) {
        p = next_track_point(p, &cx, &cy);
        int ex = off_x + (int)(((int64_t)(get_pixel_coord(cx) - min_x) * scale) >> 16);
        int ey = off_y + draw_h - (int)(((int64_t)(get_pixel_coord(cy) - min_y) * scale) >> 16);
        draw_line(prev_x, prev_y, ex, ey, color);
        prev_x = ex; prev_y = ey;
    }
}

// Fast magnitude approximation (matches ref GamePhysics::getSmthLikeMaxAbs):
// ~0.983*max(|x|,|y|) + 0.431*min(|x|,|y|)
IWRAM_FN static int fast_hypot(int x, int y) {
    int ax = x < 0 ? -x : x;
    int ay = y < 0 ? -y : y;
    int mx = ax > ay ? ax : ay;
    int mn = ax > ay ? ay : ax;
    return (int)(((int64_t)64448 * mx) >> 16) + (int)(((int64_t)28224 * mn) >> 16);
}

// 3D ribbon parameters (screen space). The far edge of each track segment is
// offset toward a vanishing "eye" point, and connector rungs are drawn between
// the near (riding) edge and the far edge, reproducing the ref renderLevel3D look.
#define RIBBON_DEPTH   16   // ribbon width in pixels
#define EYE_HEIGHT     90   // eye distance above screen center

// Project an internal track point to its near (riding) and far (ribbon) screen
// coordinates. Returns near in (*nx,*ny), far in (*fx,*fy).
IWRAM_FN static void project_point(int32_t ix, int32_t iy, int ox, int oy,
                          int* nx, int* ny, int* fx, int* fy) {
    int sx = get_pixel_coord(ix) + ox;
    int sy = oy - get_pixel_coord(iy);
    int vx = SCREEN_WIDTH / 2;
    int vy = SCREEN_HEIGHT / 2 - EYE_HEIGHT;
    int ddx = vx - sx;
    int ddy = vy - sy;
    int m = fast_hypot(ddx, ddy);
    if (m < 1) m = 1;
    *nx = sx;
    *ny = sy;
    *fx = sx + ddx * RIBBON_DEPTH / m;
    *fy = sy + ddy * RIBBON_DEPTH / m;
}

IWRAM_FN static int seg_offscreen(int x1, int x2) {
    return (x1 < -RIBBON_DEPTH && x2 < -RIBBON_DEPTH) ||
           (x1 >= SCREEN_WIDTH + RIBBON_DEPTH && x2 >= SCREEN_WIDTH + RIBBON_DEPTH);
}

// Draw one pole of an animated flag (pole line + sprite) at base point (x,y),
// using the start (pennant) or finish (checkered) frame from
// ref/assets/sprites.png. Each flag has two poles at different depths.
static void flag_pole(int x, int y, int finish) {
    int slot = (s_flag_tick >> 3) & 3;
    const color_t* frame = finish
        ? flag_finish_frames[flag_finish_anim[slot]]
        : flag_start_frames[flag_start_anim[slot]];
    draw_line(x, y, x, y - 28, COLOR(0, 0, 0));
    draw_sprite(x + 1, y - 28, frame, FLAG_W, FLAG_H);
}

// Draw the start+finish flag poles for one depth layer. near==0 draws the far
// (ribbon-side) poles, called with the track so they sit behind the moto;
// near==1 draws the near (riding-side) poles, called after the moto so they sit
// in front of it — the rider passes between the two poles of each flag.
static void draw_flag_layer(const TrackGeom* g, int ox, int oy, int near) {
    const int flag_idx[2] = { g->start_flag_idx, g->finish_flag_idx };
    for (int k = 0; k < 2; k++) {       // k: 0 = start flag, 1 = finish flag
        int s = flag_idx[k];
        if (s < 0 || s >= g->count) continue;
        int nx, ny, fx, fy;
        project_point(g->px[s], g->py[s], ox, oy, &nx, &ny, &fx, &fy);
        if (nx < -RIBBON_DEPTH || nx >= SCREEN_WIDTH + RIBBON_DEPTH) continue;
        if (near) flag_pole(nx, ny, k);
        else      flag_pole(fx, fy, k);
    }
}

// Binary search for the first point whose near (screen) X is >= target_px.
// Relies on px[] being strictly increasing (guaranteed by load_level).
IWRAM_FN static int first_point_at(const int* px, int count, int ox, int target_px) {
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if (get_pixel_coord(px[mid]) + ox < target_px) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

IWRAM_FN void draw_track(const uint8_t* data, int cam_x, int cam_y) {
    if (*data != 0x33) return;
    s_flag_tick++;

    // All geometry is precomputed once in load_level(); per frame we only
    // project the points inside the visible X window.
    const TrackGeom* g = physics_get_track_geom();
    if (g->count < 2) return;
    const int* px = g->px;
    const int* py = g->py;

    int ox = SCREEN_WIDTH / 2 - cam_x;
    int oy = SCREEN_HEIGHT / 2 + cam_y;

    // Window of points whose segments can touch the screen. Start one point to
    // the left so the segment entering from off-screen is drawn.
    int start = first_point_at(px, g->count, ox, -RIBBON_DEPTH);
    if (start > 0) start--;

    int pnx, pny, pfx, pfy;
    project_point(px[start], py[start], ox, oy, &pnx, &pny, &pfx, &pfy);

    for (int i = start + 1; i < g->count; i++) {
        int nnx, nny, nfx, nfy;
        project_point(px[i], py[i], ox, oy, &nnx, &nny, &nfx, &nfy);

        if (!seg_offscreen(pnx, nnx)) {
            // Far edge (back) and connector rung, then bright near (riding) edge.
            draw_line(pfx, pfy, nfx, nfy, COLOR(0, 13, 0));
            draw_line(pnx, pny, pfx, pfy, COLOR(0, 13, 0));
            draw_line(pnx, pny, nnx, nny, COLOR(0, 31, 0));
        }

        // Everything past the right edge is off-screen (X is increasing): the
        // segment crossing it is drawn above, then we stop.
        if (nnx >= SCREEN_WIDTH + RIBBON_DEPTH) break;

        pnx = nnx; pny = nny; pfx = nfx; pfy = nfy;
    }

    // Far (ribbon-side) poles draw with the track, behind the moto.
    draw_flag_layer(g, ox, oy, 0);
}

// Near (riding-side) flag poles. Drawn in a separate pass after draw_bike so the
// pole on the rider's side stays in front of the moto (single-buffered MODE3 is
// a painter's model: later draws win), while the far poles drawn in draw_track
// stay behind it — so the rider passes between the two poles of each flag.
IWRAM_FN void draw_track_flags(const uint8_t* data, int cam_x, int cam_y) {
    if (*data != 0x33) return;

    const TrackGeom* g = physics_get_track_geom();
    if (g->count < 2) return;

    int ox = SCREEN_WIDTH / 2 - cam_x;
    int oy = SCREEN_HEIGHT / 2 + cam_y;

    draw_flag_layer(g, ox, oy, 1);
}
