#include "level.h"
#include "graphics.h"

int convert_coord(int32_t raw) {
    // raw comes from read_be32, so it's a 32-bit signed integer.
    // In reference: addPointSimple(var << 16 >> 3)
    // To get pixel: point << 3 >> 16
    return (int)((int32_t)raw << 16) >> 3;
}

int get_pixel_coord(int32_t internal) {
    return (int)((int32_t)internal << 3) >> 16;
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
            if (start_y) *start_y = get_pixel_coord(cur_y);
            found_start = 1;
        }
        if (!found_finish && cur_x > finish_threshold) {
            if (finish_y) *finish_y = get_pixel_coord(cur_y);
            found_finish = 1;
        }
    }
}

void draw_track(const uint8_t* data, int cam_x, int cam_y) {
    const uint8_t* p = data;
    if (*p != 0x33) return;
    p++;

    // These are already in internal F13 format: (value << 16) >> 3
    int32_t start_threshold = (int32_t)read_be32(p); p += 4;
    p += 4; // skip start_y
    int32_t finish_threshold = (int32_t)read_be32(p); p += 4;
    p += 4; // skip finish_y

    uint16_t points_count = read_be16(p); p += 2;
    
    int32_t cur_x = convert_coord(read_be32(p)); p += 4;
    int32_t cur_y = convert_coord(read_be32(p)); p += 4;

    int ox = SCREEN_WIDTH / 2 - cam_x;
    int oy = SCREEN_HEIGHT / 2 + cam_y;

    const uint8_t* p_flags = p;
    int32_t loop_x = cur_x;
    int32_t loop_y = cur_y;

    int32_t start_flag_x = -1, start_flag_y = 0;
    int32_t finish_flag_x = -1, finish_flag_y = 0;
    int found_start = 0, found_finish = 0;

    for (int i = 0; i < points_count - 1; i++) {
        int8_t dx = (int8_t)*p_flags++;
        if (dx == -1) {
            loop_x = convert_coord(read_be32(p_flags)); p_flags += 4;
            loop_y = convert_coord(read_be32(p_flags)); p_flags += 4;
            continue;
        } else {
            int8_t dy = (int8_t)*p_flags++;
            loop_x += convert_coord(dx);
            loop_y += convert_coord(dy);
        }
        if (!found_start && loop_x > start_threshold) {
            start_flag_x = loop_x;
            start_flag_y = loop_y;
            found_start = 1;
        }
        if (!found_finish && loop_x > finish_threshold) {
            finish_flag_x = loop_x;
            finish_flag_y = loop_y;
            found_finish = 1;
        }
    }

    // Draw flags
    if (found_start) {
        int sfx = get_pixel_coord(start_flag_x) + ox;
        int sfy = oy - get_pixel_coord(start_flag_y);
        if (sfx >= -20 && sfx < SCREEN_WIDTH + 20) {
            draw_line(sfx, sfy, sfx, sfy - 20, COLOR(0, 0, 0));
            draw_rect(sfx, sfy - 20, 8, 5, COLOR(0, 31, 0));
            draw_string(sfx - 10, sfy + 2, "START", COLOR(0, 0, 0));
        }
    }
    if (found_finish) {
        int ffx = get_pixel_coord(finish_flag_x) + ox;
        int ffy = oy - get_pixel_coord(finish_flag_y);
        if (ffx >= -20 && ffx < SCREEN_WIDTH + 20) {
            draw_line(ffx, ffy, ffx, ffy - 20, COLOR(0, 0, 0));
            draw_rect(ffx, ffy - 20, 8, 5, COLOR(31, 0, 0));
            draw_string(ffx - 10, ffy + 2, "FINISH", COLOR(0, 0, 0));
        }
    }

    for (int i = 0; i < points_count - 1; i++) {
        int32_t next_x, next_y;
        int8_t dx = (int8_t)*p++;
        if (dx == -1) {
            cur_x = convert_coord(read_be32(p)); p += 4;
            cur_y = convert_coord(read_be32(p)); p += 4;
        } else {
            int8_t dy = (int8_t)*p++;
            next_x = cur_x + convert_coord(dx);
            next_y = cur_y + convert_coord(dy);
            
            int x1 = get_pixel_coord(cur_x) + ox;
            int x2 = get_pixel_coord(next_x) + ox;

            // Horizontal clipping
            if (!((x1 < 0 && x2 < 0) || (x1 >= SCREEN_WIDTH && x2 >= SCREEN_WIDTH))) {
                int y1 = oy - get_pixel_coord(cur_y);
                int y2 = oy - get_pixel_coord(next_y);
                draw_line(x1, y1, x2, y2, COLOR(0, 31, 0));
            }

            cur_x = next_x;
            cur_y = next_y;
        }
    }
}
