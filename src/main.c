#include "gba.h"
#include "graphics.h"
#include "level.h"
#include "physics.h"
#include "sound.h"
#include "save.h"
#include "gd_assets.h"
#include <stdlib.h>

enum State {
    STATE_INTRO,
    STATE_SPLASH,
    STATE_MENU_HARDNESS,
    STATE_MENU_TRACK,
    STATE_SETTINGS,
    STATE_CONFIRM_RESET,
    STATE_ABOUT,
    STATE_TRACK_VIEW,
    STATE_GAME,
    STATE_FINISHED
};

// Which buttons lean the bike. Chosen on the settings screen.
enum TiltMode { TILT_DPAD, TILT_SHOULDERS };
static int tilt_mode = TILT_DPAD;

// Settings screen cursor options.
enum { SET_TILT, SET_RESET, SET_ABOUT, SET_COUNT };
static int settings_cursor = SET_TILT;

Bike player_bike;

// Pixel width of a string in the 5x7 font (6px advance per glyph).
static int str_px_width(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n * 6;
}

// Draw a string horizontally centered on the screen.
static void draw_string_centered(int y, const char* s, color_t color) {
    draw_string((SCREEN_WIDTH - str_px_width(s)) / 2, y, s, color);
}

// Centered menu row with a blinking selection cursor to its left.
static void draw_menu_row(int y, const char* s, color_t color, int selected, int blink) {
    int x = (SCREEN_WIDTH - str_px_width(s)) / 2;
    draw_string(x, y, s, color);
    if (selected && blink) draw_string(x - 12, y, ">", color);
}

// Minimal string builders (no libc): append text / a non-negative int.
static char* str_cat(char* d, const char* s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char* str_num(char* d, int n) {
    char tmp[12]; int k = 0;
    if (n == 0) tmp[k++] = '0';
    while (n > 0) { tmp[k++] = '0' + n % 10; n /= 10; }
    while (k > 0) *d++ = tmp[--k];
    *d = 0; return d;
}

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
    // Enable ROM prefetch buffer: sequential ROM reads drop from 4 to 1 wait state.
    // Bit 14 = prefetch enable; rest are sensible wait state values.
    REG_WAITCNT = 0x4317;

    // EWRAM to 1 wait state (BIOS default is 2). The frame back buffer lives in
    // EWRAM and is both drawn into and DMA-copied to VRAM each frame, so this
    // ~33% RAM speedup directly buys back the cost of double-buffering.
    REG_MEMCTL = 0x0E000020;

    // Free-running timer drives the fixed-timestep loop: physics always advances
    // at 60 Hz regardless of how many frames the renderer actually presents.
    REG_TM0CNT_L = 0;
    REG_TM0CNT_H = TM_ENABLE | TM_PRESCALE_1024;

    REG_DISPCNT = MODE3 | BG2_ENABLE;
    sound_init();
    save_load();
    debug_init();

    const uint8_t* mrg = _binary_levels_mrg_start;

    int level_idx = 0;
    int track_idx = 0;
    int cam_x = 0, cam_y = 0;
    int frame = 0;
    enum State state = STATE_INTRO;
    const uint8_t* cur_track = NULL; // cached; set when entering STATE_GAME
    
    uint16_t prev_keys = 0;
    int timer = 0;
    int finish_time = 0;
    int finish_new_best = 0;   // did the last finish set a record
    int finish_unlocked = 0;   // did the last finish unlock new content
    int finish_x = 1000000;
    int prev_state = -1;   // force a redraw on the first frame
    int prev_blink = -1;

    uint16_t tm_prev = REG_TM0CNT_L;  // last timer sample
    int tm_accum = 0;                 // unspent time, in timer ticks

    while (1) {
        uint16_t keys = ~REG_KEYINPUT & 0x03FF;
        uint16_t keys_pressed = keys & ~prev_keys;
        prev_keys = keys;

        // Fixed-timestep clock: how many 60 Hz sim steps are owed for the real
        // time elapsed since last loop. At 30 fps render this is 2 per frame,
        // keeping the simulation at full speed. Clamp to avoid a catch-up spiral
        // after a long stall (e.g. level load).
        uint16_t tm_now = REG_TM0CNT_L;
        tm_accum += (uint16_t)(tm_now - tm_prev);
        tm_prev = tm_now;
        int steps = tm_accum / TICKS_PER_STEP;
        if (steps > 4) { steps = 4; tm_accum = 0; }
        else tm_accum -= steps * TICKS_PER_STEP;

        if (state == STATE_INTRO) {
            // Auto-advance to the splash after ~5 seconds (60 fps).
            if (frame >= 300) state = STATE_SPLASH;
        } else if (state == STATE_SPLASH) {
            if (keys_pressed & (KEY_START | KEY_A | KEY_B)) {
                state = STATE_MENU_HARDNESS;
            }
        } else if (state == STATE_MENU_HARDNESS) {
            if (keys_pressed & KEY_UP) {
                if (level_idx > 0) level_idx--;
            }
            if (keys_pressed & KEY_DOWN) {
                if (level_idx < 2) level_idx++;
            }
            if (keys_pressed & (KEY_START | KEY_A)) {
                int prev = level_idx > 0 ? level_track_count(mrg, level_idx - 1) : 0;
                if (league_unlocked(level_idx, prev)) {
                    state = STATE_MENU_TRACK;
                    track_idx = 0;
                }
            }
            if (keys_pressed & KEY_SELECT) {
                state = STATE_SETTINGS;
                settings_cursor = SET_TILT;  // start on the safe option
            }
        } else if (state == STATE_SETTINGS) {
            // UP/DOWN move between options; B/SELECT returns to the menu.
            if (keys_pressed & KEY_UP)   settings_cursor = (settings_cursor + SET_COUNT - 1) % SET_COUNT;
            if (keys_pressed & KEY_DOWN) settings_cursor = (settings_cursor + 1) % SET_COUNT;
            if (settings_cursor == SET_TILT) {
                if (keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_A)) {
                    tilt_mode = (tilt_mode == TILT_DPAD) ? TILT_SHOULDERS : TILT_DPAD;
                }
            } else if (settings_cursor == SET_RESET) {
                if (keys_pressed & KEY_A) state = STATE_CONFIRM_RESET;
            } else if (settings_cursor == SET_ABOUT) {
                if (keys_pressed & KEY_A) state = STATE_ABOUT;
            }
            if (keys_pressed & (KEY_B | KEY_SELECT)) {
                state = STATE_MENU_HARDNESS;
            }
        } else if (state == STATE_ABOUT) {
            if (keys_pressed & (KEY_B | KEY_SELECT | KEY_A | KEY_START)) {
                state = STATE_SETTINGS;
            }
        } else if (state == STATE_CONFIRM_RESET) {
            // Default to NO: only A confirms; anything else cancels.
            if (keys_pressed & KEY_A) {
                save_reset();
                state = STATE_SETTINGS;
            } else if (keys_pressed & (KEY_B | KEY_SELECT | KEY_START)) {
                state = STATE_SETTINGS;
            }
        } else if (state == STATE_MENU_TRACK) {
            if (keys_pressed & KEY_UP) {
                if (track_idx > 0) track_idx--;
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
                if (track_idx < count - 1) track_idx++;
            }
            if (keys_pressed & (KEY_START | KEY_A)) {
              int prev = level_idx > 0 ? level_track_count(mrg, level_idx - 1) : 0;
              if (track_unlocked(level_idx, track_idx, prev)) {
                state = STATE_GAME;
                cur_track = get_track_data(mrg, level_idx, track_idx);
                physics_set_league(level_idx);
                init_bike(&player_bike, cur_track);
                update_physics(&player_bike, cur_track, 0);
                cam_x = get_pixel_coord(player_bike.x);
                cam_y = get_pixel_coord(player_bike.y);
                timer = 0;
                get_track_flags(cur_track, NULL, NULL, &finish_x, NULL);
                tm_accum = 0;  // don't dump menu idle time into the first tick
              }
            }
            if (keys_pressed & (KEY_SELECT | KEY_B)) {
                state = STATE_MENU_HARDNESS;
            }
        } else if (state == STATE_GAME) {
            // Run the simulation at a fixed 60 Hz: `steps` ticks this frame
            // (2 when rendering at 30 fps). timer counts sim steps, so elapsed
            // time stays correct independent of the render rate.
            // Remap tilt to the shoulder buttons if selected: physics reads
            // LEFT/RIGHT for lean, so feed L/R into those bits instead.
            uint16_t pk = keys;
            if (tilt_mode == TILT_SHOULDERS) {
                pk &= ~(KEY_LEFT | KEY_RIGHT);
                if (keys & KEY_L) pk |= KEY_LEFT;
                if (keys & KEY_R) pk |= KEY_RIGHT;
            }
            for (int s = 0; s < steps && state == STATE_GAME; s++) {
                update_physics(&player_bike, cur_track, pk);
                if (player_bike.crash) {
                    sound_play_crash();
                    init_bike(&player_bike, cur_track);
                    update_physics(&player_bike, cur_track, 0);
                    timer = 0;
                }
                timer++;
                if (get_pixel_coord(player_bike.x) >= finish_x) {
                    state = STATE_FINISHED;
                    finish_time = timer;
                    // Record progress: mark complete, keep best time, and note
                    // whether a new track or league just opened up.
                    int tc = level_track_count(mrg, level_idx);
                    int was_done = g_save.completed[level_idx][track_idx];
                    finish_new_best = record_finish(level_idx, track_idx, finish_time);
                    finish_unlocked = 0;
                    if (!was_done) {
                        if (track_idx + 1 < tc) {
                            finish_unlocked = 1;            // next track now open
                        } else if (level_idx + 1 < NUM_LEAGUES) {
                            int all = 1;                    // last track: league done?
                            for (int t = 0; t < tc; t++)
                                if (!g_save.completed[level_idx][t]) { all = 0; break; }
                            finish_unlocked = all;          // next league now open
                        }
                    }
                }
            }
            cam_x = get_pixel_coord(player_bike.x);
            cam_y = get_pixel_coord(player_bike.y);

            if (keys_pressed & KEY_SELECT) {
                state = STATE_MENU_TRACK;
            }
        } else if (state == STATE_FINISHED) {
            if (keys_pressed & (KEY_START | KEY_A | KEY_B | KEY_SELECT)) {
                state = STATE_MENU_TRACK;
            }
        } else {
            // Navigation over track
            int speed = (keys & KEY_A) ? 8 : 4;
            if (keys & KEY_RIGHT) cam_x += speed;
            if (keys & KEY_LEFT)  cam_x -= speed;
            if (keys & KEY_UP)    cam_y += speed;
            if (keys & KEY_DOWN)  cam_y -= speed;

            if (keys_pressed & (KEY_SELECT | KEY_B)) {
                state = STATE_MENU_TRACK;
            }
        }

        frame++;
        // ~0.5 s on / 0.5 s off, drives blinking prompts and the menu cursor.
        int blink = ((frame / 30) & 1) == 0;
        int blinking_state = (state == STATE_SPLASH ||
                              state == STATE_MENU_HARDNESS ||
                              state == STATE_MENU_TRACK ||
                              state == STATE_SETTINGS);
        // Only redraw when something visible can change: the game animates every
        // frame, a blinking screen on each toggle, and any screen on entry or a
        // key press. Static screens (intro, finished) are drawn once and skip
        // both the redraw and the back-buffer->VRAM copy below, so an idle menu
        // costs almost nothing.
        int redraw = (state == STATE_GAME)
                   || ((int)state != prev_state)
                   || keys_pressed
                   || (blinking_state && blink != prev_blink);
        prev_state = state;
        prev_blink = blink;

        if (redraw) {
            clear_screen(state == STATE_INTRO ? COLOR(0, 0, 0) : COLOR(31, 31, 31));

            if (state == STATE_INTRO) {
                draw_string_centered(76, "Remake by rgaming.com.ua", COLOR(0, 31, 0));
            } else if (state == STATE_SPLASH) {
                draw_sprite((SCREEN_WIDTH - SPLASH_IMG_W) / 2, 50, splash_img, SPLASH_IMG_W, SPLASH_IMG_H);
                if (blink) draw_string(87, 120, "PRESS START", COLOR(0, 0, 0));
            } else if (state == STATE_MENU_HARDNESS) {
                static const char* league_names[3] = { "100cc", "175cc", "220cc" };
                // Title "ReGravity Defied": "Re" green, the rest black, centered.
                int title_x = (SCREEN_WIDTH - str_px_width("ReGravity Defied")) / 2;
                draw_string(title_x, 20, "Re", COLOR(0, 31, 0));
                draw_string(title_x + str_px_width("Re"), 20, "Gravity Defied", COLOR(0, 0, 0));
                for (int i = 0; i < 3; i++) {
                    int prev = i > 0 ? level_track_count(mrg, i - 1) : 0;
                    int unlocked = league_unlocked(i, prev);
                    int tc = level_track_count(mrg, i);
                    int done = 0;
                    for (int t = 0; t < tc; t++) done += g_save.completed[i][t];

                    char buf[24];
                    char* e = str_cat(buf, league_names[i]);
                    if (!unlocked) {
                        str_cat(e, "  LOCKED");
                    } else {
                        e = str_cat(e, "  "); e = str_num(e, done);
                        e = str_cat(e, "/"); str_num(e, tc);
                    }
                    color_t color = !unlocked ? COLOR(15, 15, 15)
                                  : (i == level_idx) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                    draw_menu_row(60 + i * 20, buf, color, i == level_idx, blink);
                }
                draw_string_centered(140, "SELECT: SETTINGS", COLOR(10, 10, 10));
            } else if (state == STATE_SETTINGS) {
                draw_string_centered(20, "SETTINGS", COLOR(0, 0, 0));

                char tbuf[28];
                char* e = str_cat(tbuf, "TILT: ");
                str_cat(e, tilt_mode == TILT_DPAD ? "D-PAD" : "L/R SHOULDERS");
                color_t tcol = (settings_cursor == SET_TILT) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                draw_menu_row(60, tbuf, tcol, settings_cursor == SET_TILT, blink);

                color_t rcol = (settings_cursor == SET_RESET) ? COLOR(31, 0, 0) : COLOR(15, 8, 8);
                draw_menu_row(80, "RESET PROGRESS", rcol, settings_cursor == SET_RESET, blink);

                color_t acol = (settings_cursor == SET_ABOUT) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                draw_menu_row(100, "ABOUT", acol, settings_cursor == SET_ABOUT, blink);

                draw_string_centered(125, "A: SELECT   UP/DOWN", COLOR(10, 10, 10));
                draw_string_centered(140, "B: BACK", COLOR(10, 10, 10));
            } else if (state == STATE_ABOUT) {
                int tx = (SCREEN_WIDTH - str_px_width("ReGravity Defied")) / 2;
                draw_string(tx, 18, "Re", COLOR(0, 31, 0));
                draw_string(tx + str_px_width("Re"), 18, "Gravity Defied", COLOR(0, 0, 0));
                draw_string_centered(44, "OPEN SOURCE PORT OF", COLOR(10, 10, 10));
                draw_string_centered(56, "GRAVITY DEFIED", COLOR(0, 0, 0));
                draw_string_centered(86, "GITHUB.COM/ANTOXA2584X", COLOR(0, 22, 0));
                draw_string_centered(98, "/REGRAVITY_DEFIED_GBA", COLOR(0, 22, 0));
                draw_string_centered(140, "B: BACK", COLOR(10, 10, 10));
            } else if (state == STATE_CONFIRM_RESET) {
                draw_rect(40, 55, 160, 50, COLOR(31, 31, 31));
                draw_rect(42, 57, 156, 46, COLOR(0, 0, 0));
                draw_string_centered(64, "ERASE ALL PROGRESS", COLOR(31, 31, 31));
                draw_string_centered(76, "AND BEST TIMES?", COLOR(31, 31, 31));
                draw_string_centered(92, "A: YES    B: NO", COLOR(31, 31, 0));
            } else if (state == STATE_MENU_TRACK) {
                draw_string_centered(10, "SELECT TRACK", COLOR(0, 0, 0));
                draw_string_centered(24, "BEST TIMES", COLOR(12, 12, 12));

                int prev = level_idx > 0 ? level_track_count(mrg, level_idx - 1) : 0;
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

                    if (t >= track_idx - 3 && t <= track_idx + 3) {
                        int unlocked = track_unlocked(level_idx, t, prev);
                        char buf[30];
                        char* e = str_cat(buf, name);
                        e = str_cat(e, "  ");
                        if (!unlocked) {
                            str_cat(e, "LOCKED");
                        } else if (g_save.best[level_idx][t]) {
                            char tb[10];
                            format_time(g_save.best[level_idx][t], tb);
                            str_cat(e, tb);
                        } else {
                            str_cat(e, "--:--.--");
                        }
                        color_t color = !unlocked ? COLOR(15, 15, 15)
                                      : (t == track_idx) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                        int y_pos = 44 + (t - (track_idx - 3)) * 12;
                        draw_menu_row(y_pos, buf, color, t == track_idx, blink);
                    }

                    while (*track_info) track_info++;
                    track_info++;
                }
            } else if (state == STATE_GAME || state == STATE_TRACK_VIEW || state == STATE_FINISHED) {
                // Draw HUD first — it sits at y=10 (top of screen). If we drew it
                // last and rendering overflowed VBlank, the scan line would have
                // already passed row 10, causing every-other-frame flickering.
                if (state == STATE_GAME) {
                    char time_buf[10];
                    format_time(timer, time_buf);
                    draw_string(10, 10, time_buf, COLOR(0, 0, 0));
                }

                draw_track(cur_track, cam_x, cam_y);
                if (state == STATE_GAME || state == STATE_FINISHED) {
                    draw_bike(&player_bike, SCREEN_WIDTH / 2 - cam_x, SCREEN_HEIGHT / 2 + cam_y);
                }

                if (state == STATE_FINISHED) {
                    draw_rect(50, 50, 140, 60, COLOR(31, 31, 31));
                    draw_rect(52, 52, 136, 56, COLOR(0, 0, 0));
                    draw_string_centered(60, "FINISHED!", COLOR(31, 31, 31));
                    char time_buf[10];
                    format_time(finish_time, time_buf);
                    draw_string_centered(74, time_buf, COLOR(0, 31, 0));
                    if (finish_unlocked)
                        draw_string_centered(88, "UNLOCKED NEXT!", COLOR(0, 31, 31));
                    else if (finish_new_best)
                        draw_string_centered(88, "NEW BEST TIME!", COLOR(31, 31, 0));
                }
            }
        }

        // All drawing above went to the off-screen back buffer (no beam race).
        // Wait for VBlank, then DMA the finished frame to VRAM — the copy
        // outruns the scan beam, so nothing tears. Skip the copy when we did
        // not redraw: VRAM already holds the last presented frame.
        vsync();
        if (redraw) present_frame();
        sound_tick();
    }

    return 0;
}
