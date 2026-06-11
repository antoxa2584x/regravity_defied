#include "gba.h"
#include "graphics.h"
#include "level.h"
#include "physics.h"
#include <stdlib.h>

enum State {
    STATE_MENU_HARDNESS,
    STATE_MENU_TRACK,
    STATE_TRACK_VIEW,
    STATE_GAME,
    STATE_FINISHED
};

Bike player_bike;

static void format_time(int frames, char* buf) {
    int total_seconds = frames / 60;
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    int centis = (frames % 60) * 100 / 60;
    
    // Simple itoa-like formatting
    buf[0] = '0' + (minutes / 10);
    buf[1] = '0' + (minutes % 10);
    buf[2] = ':';
    buf[3] = '0' + (seconds / 10);
    buf[4] = '0' + (seconds % 10);
    buf[5] = '.';
    buf[6] = '0' + (centis / 10);
    buf[7] = '0' + (centis % 10);
    buf[8] = 0;
}

int main() {
    REG_DISPCNT = MODE3 | BG2_ENABLE;

    const uint8_t* mrg = _binary_levels_mrg_start;
    
    int level_idx = 0;
    int track_idx = 0;
    int cam_x = 0, cam_y = 0;
    int update = 1;
    enum State state = STATE_MENU_HARDNESS;
    
    uint16_t prev_keys = 0;
    int timer = 0;
    int finish_time = 0;
    int finish_x = 1000000;

    while (1) {
        uint16_t keys = ~REG_KEYINPUT & 0x03FF;
        uint16_t keys_pressed = keys & ~prev_keys;
        prev_keys = keys;

        if (state == STATE_MENU_HARDNESS) {
            if (keys_pressed & KEY_UP) {
                if (level_idx > 0) { level_idx--; update = 1; }
            }
            if (keys_pressed & KEY_DOWN) {
                if (level_idx < 2) { level_idx++; update = 1; }
            }
            if (keys_pressed & (KEY_START | KEY_A)) {
                state = STATE_MENU_TRACK;
                track_idx = 0;
                update = 1;
            }
        } else if (state == STATE_MENU_TRACK) {
            if (keys_pressed & KEY_UP) {
                if (track_idx > 0) { track_idx--; update = 1; }
            }
            if (keys_pressed & KEY_DOWN) {
                int count = 0;
                const uint8_t* p = mrg;
                for (int l = 0; l <= level_idx; l++) {
                    count = read_be32(p);
                    if (l < level_idx) {
                        p += 4;
                        for (int t = 0; t < count; t++) {
                            p += 4;
                            while (*p++) ;
                        }
                    }
                }
                if (track_idx < count - 1) { track_idx++; update = 1; }
            }
            if (keys_pressed & (KEY_START | KEY_A)) {
                state = STATE_GAME;
                const uint8_t* track_data = get_track_data(mrg, level_idx, track_idx);
                physics_set_league(level_idx); // 0=100cc, 1=175cc, 2=220cc
                init_bike(&player_bike, track_data);
                // Immediately update physics once to snap to ground if close
                update_physics(&player_bike, track_data, 0);
                cam_x = get_pixel_coord(player_bike.x);
                cam_y = get_pixel_coord(player_bike.y);
                update = 1;
                timer = 0;
                get_track_flags(track_data, NULL, NULL, &finish_x, NULL);
            }
            if (keys_pressed & (KEY_SELECT | KEY_B)) {
                state = STATE_MENU_HARDNESS;
                update = 1;
            }
        } else if (state == STATE_GAME) {
            update_physics(&player_bike, get_track_data(mrg, level_idx, track_idx), keys);
            if (player_bike.crash) {
                // Restart on crash
                const uint8_t* track_data = get_track_data(mrg, level_idx, track_idx);
                init_bike(&player_bike, track_data);
                update_physics(&player_bike, track_data, 0);
                timer = 0;
            }
            cam_x = get_pixel_coord(player_bike.x);
            cam_y = get_pixel_coord(player_bike.y);
            update = 1;
            timer++;

            if (get_pixel_coord(player_bike.x) >= finish_x) {
                state = STATE_FINISHED;
                finish_time = timer;
            }

            if (keys_pressed & (KEY_SELECT | KEY_B)) {
                state = STATE_MENU_TRACK;
                update = 1;
            }
        } else if (state == STATE_FINISHED) {
            if (keys_pressed & (KEY_START | KEY_A | KEY_B | KEY_SELECT)) {
                state = STATE_MENU_TRACK;
                update = 1;
            }
        } else {
            // Navigation over track
            int speed = (keys & KEY_A) ? 8 : 4;
            if (keys & KEY_RIGHT) { cam_x += speed; update = 1; }
            if (keys & KEY_LEFT)  { cam_x -= speed; update = 1; }
            if (keys & KEY_UP)    { cam_y += speed; update = 1; }
            if (keys & KEY_DOWN)  { cam_y -= speed; update = 1; }

            if (keys_pressed & (KEY_SELECT | KEY_B)) {
                state = STATE_MENU_TRACK;
                update = 1;
            }
        }

        if (update || state == STATE_GAME) {
            vsync();
            // Clear screen manually with 32-bit writes (optimized in graphics.c)
            clear_screen(COLOR(31, 31, 31)); 
            
            if (state == STATE_MENU_HARDNESS) {
                static const char* league_names[3] = { "100cc", "175cc", "220cc" };
                draw_string(85, 20, "SELECT LEAGUE", COLOR(0, 0, 0));
                for (int i = 0; i < 3; i++) {
                    color_t color = (i == level_idx) ? COLOR(0, 31, 0) : COLOR(10, 10, 10);
                    draw_string(100, 60 + i * 20, league_names[i], color);
                    if (i == level_idx) {
                        draw_string(85, 60 + i * 20, ">", color);
                    }
                }
            } else if (state == STATE_MENU_TRACK) {
                draw_string(70, 10, "SELECT TRACK", COLOR(0, 0, 0));
                
                const uint8_t* p = mrg;
                for (int l = 0; l < level_idx; l++) {
                    uint32_t count = read_be32(p);
                    p += 4;
                    for (int t = 0; t < (int)count; t++) {
                        p += 4;
                        while (*p++) ;
                    }
                }
                uint32_t count = read_be32(p);
                const uint8_t* track_info = p + 4;
                for (int t = 0; t < (int)count; t++) {
                    track_info += 4;
                    const char* name = (const char*)track_info;
                    
                    color_t color = (t == track_idx) ? COLOR(0, 31, 0) : COLOR(10, 10, 10);
                    if (t >= track_idx - 3 && t <= track_idx + 3) {
                        int y_pos = 40 + (t - (track_idx - 3)) * 12;
                        if (t == track_idx) draw_string(40, y_pos, ">", color);
                        draw_string(55, y_pos, name, color);
                    }

                    while (*track_info) track_info++;
                    track_info++;
                }
            } else if (state == STATE_GAME || state == STATE_TRACK_VIEW || state == STATE_FINISHED) {
                const uint8_t* track_data = get_track_data(mrg, level_idx, track_idx);
                draw_track(track_data, cam_x, cam_y);
                if (state == STATE_GAME || state == STATE_FINISHED) {
                    draw_bike(&player_bike, SCREEN_WIDTH / 2 - cam_x, SCREEN_HEIGHT / 2 + cam_y);
                }
                
                if (state == STATE_GAME) {
                    char time_buf[10];
                    format_time(timer, time_buf);
                    // Draw UI after bike to ensure it's on top and reduces potential visible tearing on critical info
                    draw_string(10, 10, time_buf, COLOR(0, 0, 0));
                } else if (state == STATE_FINISHED) {
                    draw_rect(60, 60, 120, 40, COLOR(31, 31, 31));
                    draw_rect(62, 62, 116, 36, COLOR(0, 0, 0));
                    draw_string(85, 70, "FINISHED!", COLOR(31, 31, 31));
                    char time_buf[10];
                    format_time(finish_time, time_buf);
                    draw_string(85, 85, time_buf, COLOR(0, 31, 0));
                }
            }
            update = 0;
        }
    }

    return 0;
}
