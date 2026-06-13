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

// Centered, outlined — used where text sits over the track or a bright panel.
static void draw_string_centered_outlined(int y, const char* s, color_t fg, color_t outline) {
    draw_string_outlined((SCREEN_WIDTH - str_px_width(s)) / 2, y, s, fg, outline);
}

// Centered menu row with a blinking selection cursor to its left.
static void draw_menu_row(int y, const char* s, color_t color, int selected, int blink) {
    int x = (SCREEN_WIDTH - str_px_width(s)) / 2;
    draw_string(x, y, s, color);
    if (selected && blink) draw_string(x - 12, y, ">", color);
}

// As draw_menu_row, but with a 1px outline (used over the animated backdrop).
static void draw_menu_row_outlined(int y, const char* s, color_t color, color_t outline, int selected, int blink) {
    int x = (SCREEN_WIDTH - str_px_width(s)) / 2;
    draw_string_outlined(x, y, s, color, outline);
    if (selected && blink) draw_string_outlined(x - 12, y, ">", color, outline);
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

// Signed difference vs. the old best, as "-S.CC" (faster) or "+S.CC" (slower).
static void format_delta(int frames, char* buf) {
    char* p = buf;
    if (frames < 0) { *p++ = '-'; frames = -frames; } else { *p++ = '+'; }
    int centis = frames * 100 / 60;
    p = str_num(p, centis / 100);     // whole seconds (may exceed 99)
    *p++ = '.';
    *p++ = '0' + (centis % 100) / 10;
    *p++ = '0' + (centis % 100) % 10;
    *p = 0;
}

// Semi-transparent waving checkered-flag backdrop. The "dark" squares are drawn
// in a faint grey over the white cleared screen (no real alpha in MODE3, so a
// light grey reads as a translucent black), and each column is displaced by a
// travelling sine wave to suggest waving cloth. Drawn behind the menu text, so
// the menu stays readable. `t` is a frame counter that drives the animation.
#define CHK_CELL 24
static void draw_checker_bg(int t) {
    // One period of a sine, ~6px amplitude (no libm on the GBA).
    static const signed char wave[16] =
        { 0, 3, 5, 6, 6, 6, 5, 3, 0, -3, -5, -6, -6, -6, -5, -3 };
    const color_t c = COLOR(58, 58, 58);   // faint grey ≈ translucent checker
    for (int col = -1; col * CHK_CELL < SCREEN_WIDTH; col++) {
        int x = col * CHK_CELL;
        int yoff = wave[(col * 2 + t / 3) & 15];   // neighbouring columns ripple
        for (int row = -1; row * CHK_CELL < SCREEN_HEIGHT + CHK_CELL; row++) {
            if (((col + row) & 1) == 0) continue;  // checkerboard: alternate cells
            draw_rect(x, row * CHK_CELL + yoff, CHK_CELL, CHK_CELL, c);
        }
    }
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
    int finish_delta = 0;      // finish_time - previous best (signed, frames)
    int finish_has_delta = 0;  // was there a previous best to compare against
    int finish_x = 1000000;
    int start_x = -1000000;    // pixel X of the start flag; timing begins when the front wheel passes it
    int timer_started = 0;     // has the front wheel crossed the start flag this run
    int attempts = 0;          // crashes + 1 on the current track (the run count)
    int crash_flash = 0;       // frames of red hit-flash still owed after a crash
    int prev_state = -1;   // force a redraw on the first frame
    int prev_blink = -1;

    uint16_t tm_prev = REG_TM0CNT_L;  // last timer sample
    int tm_accum = 0;                 // unspent time, in timer ticks
    uint32_t clock_ticks = 0;         // wall-clock tick count (fps-independent)

    while (1) {
        uint16_t keys = ~REG_KEYINPUT & 0x03FF;
        uint16_t keys_pressed = keys & ~prev_keys;
        prev_keys = keys;

        // Fixed-timestep clock: how many 60 Hz sim steps are owed for the real
        // time elapsed since last loop. At 30 fps render this is 2 per frame,
        // keeping the simulation at full speed. Clamp to avoid a catch-up spiral
        // after a long stall (e.g. level load).
        uint16_t tm_now = REG_TM0CNT_L;
        uint16_t dt = (uint16_t)(tm_now - tm_prev);
        tm_accum += dt;
        clock_ticks += dt;            // drives the blink at a steady wall-clock rate
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
                if (level_idx < NUM_LEAGUES - 1) level_idx++;
            }
            if (keys_pressed & (KEY_START | KEY_A)) {
                if (league_unlocked(mrg, level_idx)) {
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
              if (track_unlocked(mrg, level_idx, track_idx)) {
                state = STATE_GAME;
                cur_track = get_track_data(mrg, level_idx, track_idx);
                physics_set_league(level_idx);
                init_bike(&player_bike, cur_track);
                update_physics(&player_bike, cur_track, 0);
                cam_x = get_pixel_coord(player_bike.x);
                cam_y = get_pixel_coord(player_bike.y);
                timer = 0;
                timer_started = 0;  // starts once the front wheel passes the start flag
                attempts = 1;       // this run; bumped on each crash-restart
                crash_flash = 0;
                get_track_flags(cur_track, &start_x, NULL, &finish_x, NULL);
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
                    timer_started = 0;
                    attempts++;
                    crash_flash = 6;   // ~0.1s red hit-flash
                }
                // Timing is gated on the front wheel (node 1): it begins the
                // frame the wheel crosses the start flag and ends when it
                // crosses the finish flag.
                int front_px = get_pixel_coord(player_bike.nodes[1].x);
                if (!timer_started && front_px >= start_x) timer_started = 1;
                if (timer_started) timer++;
                if (front_px >= finish_x) {
                    state = STATE_FINISHED;
                    finish_time = timer;
                    // Record progress: mark complete, keep best time, and note
                    // whether a new track or league just opened up.
                    int tc = level_track_count(mrg, level_idx);
                    int gidx = global_track_index(mrg, level_idx, track_idx);
                    int base = gidx - track_idx;            // first track of league
                    int was_done = save_completed(gidx);
                    // Capture the prior best *before* record_finish overwrites it
                    // so the finish screen can show the delta.
                    uint32_t prev_best = save_best(gidx);
                    finish_has_delta = prev_best != 0;
                    finish_delta = (int)finish_time - (int)prev_best;
                    finish_new_best = record_finish(gidx, finish_time);
                    finish_unlocked = 0;
                    if (!was_done) {
                        if (track_idx + 1 < tc) {
                            finish_unlocked = 1;            // next track now open
                        } else if (level_idx + 1 < NUM_LEAGUES) {
                            int all = 1;                    // last track: league done?
                            for (int t = 0; t < tc; t++)
                                if (!save_completed(base + t)) { all = 0; break; }
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
        // Timed off the wall-clock (TM0 = 16384 Hz, so 8192 ticks = 0.5 s) rather
        // than the frame count, so the blink rate is steady even when a heavy
        // screen (e.g. the animated league menu) renders below 60 fps.
        int blink = ((clock_ticks / 8192) & 1) == 0;
        int blinking_state = (state == STATE_SPLASH ||
                              state == STATE_MENU_HARDNESS ||
                              state == STATE_MENU_TRACK ||
                              state == STATE_SETTINGS);
        // Only redraw when something visible can change: the game animates every
        // frame, a blinking screen on each toggle, and any screen on entry or a
        // key press. Static screens (intro, finished) are drawn once and skip
        // both the redraw and the back-buffer->VRAM copy below, so an idle menu
        // costs almost nothing.
        // A screen change (not the very first frame) gets a fade transition.
        // The finish modal is excluded: it should pop up instantly over the
        // frozen track rather than fading the gameplay away.
        int transition = (int)state != prev_state;
        int do_fade = transition && prev_state >= 0 && state != STATE_FINISHED;
        int redraw = (state == STATE_GAME)
                   || (state == STATE_MENU_HARDNESS)   // animated checker backdrop
                   || transition
                   || keys_pressed
                   || (blinking_state && blink != prev_blink);
        prev_state = state;
        prev_blink = blink;

        // Fade the outgoing screen (still in VRAM) to black before drawing the
        // new one over it; the matching fade-in runs after present_frame below.
        if (do_fade) fade_out();

        if (redraw) {
            clear_screen(state == STATE_INTRO ? COLOR(0, 0, 0) : COLOR(31, 31, 31));

            if (state == STATE_INTRO) {
                draw_string_centered(76, "Remake by rgaming.com.ua", COLOR(0, 31, 0));
            } else if (state == STATE_SPLASH) {
                draw_sprite((SCREEN_WIDTH - SPLASH_IMG_W) / 2, 50, splash_img, SPLASH_IMG_W, SPLASH_IMG_H);
                if (blink) draw_string(87, 120, "PRESS START", COLOR(0, 0, 0));
            } else if (state == STATE_MENU_HARDNESS) {
                static const char* league_names[3] = { "100cc", "175cc", "220cc" };
                draw_checker_bg(frame);   // animated semi-transparent checkered flag
                // Title "ReGravity Defied": "Re" green, the rest black, centered. 2x scale.
                // White 1px outline keeps it readable over the checkered backdrop.
                int title_x = (SCREEN_WIDTH - str_px_width("ReGravity Defied") * 2) / 2;
                draw_string_scaled_outlined(title_x, 18, "Re", COLOR(0, 31, 0), COLOR(31, 31, 31), 2);
                draw_string_scaled_outlined(title_x + str_px_width("Re") * 2, 18, "Gravity Defied", COLOR(0, 0, 0), COLOR(31, 31, 31), 2);
                for (int i = 0; i < NUM_LEAGUES; i++) {
                    int unlocked = league_unlocked(mrg, i);
                    int tc = level_track_count(mrg, i);
                    int base = global_track_index(mrg, i, 0);
                    int done = 0;
                    for (int t = 0; t < tc; t++) done += save_completed(base + t);

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
                    int row_y = 60 + i * 20;
                    draw_menu_row_outlined(row_y, buf, color, COLOR(31, 31, 31), i == level_idx, blink);
                    // Re-stamp the LOCKED word in red over the grey row.
                    if (!unlocked) {
                        int row_x = (SCREEN_WIDTH - str_px_width(buf)) / 2;
                        draw_string_outlined(row_x + str_px_width(league_names[i]) + 12, row_y,
                                             "LOCKED", COLOR(31, 8, 8), COLOR(31, 31, 31));
                    }
                }
                draw_string_centered_outlined(140, "SELECT: SETTINGS", COLOR(10, 10, 10), COLOR(31, 31, 31));
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
                int tx = (SCREEN_WIDTH - str_px_width("ReGravity Defied") * 2) / 2;
                draw_string_scaled(tx, 14, "Re", COLOR(0, 31, 0), 2);
                draw_string_scaled(tx + str_px_width("Re") * 2, 14, "Gravity Defied", COLOR(0, 0, 0), 2);
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
                // Left rail = scrolling list of names; right pane = detail card
                // (best time + medal + status) for the highlighted track.
                draw_string_centered(8, "SELECT TRACK", COLOR(0, 0, 0));
                draw_line(120, 22, 120, 140, COLOR(18, 18, 18));   // rail/pane divider

                int base = global_track_index(mrg, level_idx, 0);  // first track of league
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
                const char* sel_name = "";   // captured for the detail pane
                for (int t = 0; t < (int)count; t++) {
                    track_info += 4;
                    const char* name = (const char*)track_info;
                    if (t == track_idx) sel_name = name;

                    // Window of 7 names around the cursor, left rail only.
                    if (t >= track_idx - 3 && t <= track_idx + 3) {
                        int unlocked = track_unlocked(mrg, level_idx, t);
                        color_t color = !unlocked ? COLOR(15, 15, 15)
                                      : (t == track_idx) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                        int y_pos = 32 + (t - (track_idx - 3)) * 14;
                        if (t == track_idx && blink) draw_string(6, y_pos, ">", color);
                        draw_string(16, y_pos, name, color);
                        if (save_completed(base + t))            // tick = cleared
                            draw_string(16 + str_px_width(name) + 4, y_pos, "*", COLOR(0, 28, 0));
                    }

                    while (*track_info) track_info++;
                    track_info++;
                }

                // ---- Detail pane (centered on x = 180) ----
                const int pcx = 180;
                int sel_unlocked = track_unlocked(mrg, level_idx, track_idx);
                uint32_t sel_best = save_best(base + track_idx);

                draw_string(pcx - str_px_width(sel_name) / 2, 30, sel_name, COLOR(0, 0, 0));
                draw_string(pcx - str_px_width("BEST") / 2, 48, "BEST", COLOR(12, 12, 12));

                char tb[10];
                if (sel_best) format_time(sel_best, tb); else str_cat(tb, "--:--.--");
                color_t tcol = sel_best ? COLOR(0, 24, 0) : COLOR(14, 14, 14);
                draw_string(pcx - str_px_width(tb) / 2, 60, tb, tcol);

                {
                    // Scaled silhouette of the whole track in a framed box.
                    // Locked tracks still show their shape, but greyed out with a
                    // LOCKED label over it instead of the usual green trace.
                    const int bw = 92, bh = 46, bx = pcx - bw / 2, by = 76;
                    draw_rect(bx - 1, by - 1, bw + 2, bh + 2, COLOR(16, 16, 16));
                    draw_rect(bx, by, bw, bh, COLOR(31, 31, 31));
                    color_t trace = sel_unlocked ? COLOR(0, 22, 0) : COLOR(18, 18, 18);
                    draw_track_preview(get_track_data(mrg, level_idx, track_idx), bx, by, bw, bh, trace);
                    if (!sel_unlocked) {
                        const char* lk = "LOCKED";
                        draw_string_outlined(pcx - str_px_width(lk) / 2, by + bh / 2 - 3,
                                             lk, COLOR(31, 8, 8), COLOR(31, 31, 31));
                    }
                }

                draw_string_centered(150, "A: START   B: BACK", COLOR(10, 10, 10));
            } else if (state == STATE_GAME || state == STATE_TRACK_VIEW || state == STATE_FINISHED) {
                draw_track(cur_track, cam_x, cam_y);
                if (state == STATE_GAME || state == STATE_FINISHED) {
                    draw_bike(&player_bike, SCREEN_WIDTH / 2 - cam_x, SCREEN_HEIGHT / 2 + cam_y);
                }

                // HUD on top of the track so the timer stays readable. Outlined
                // so it stays legible over track lines and the rider.
                if (state == STATE_GAME) {
                    char time_buf[10];
                    format_time(timer, time_buf);
                    draw_string_outlined(10, 10, time_buf, COLOR(0, 0, 0), COLOR(31, 31, 31));

                    // Run counter, top-right ("x3" = on the third attempt).
                    char run_buf[16];
                    char* e = str_cat(run_buf, "x");
                    str_num(e, attempts);
                    draw_string_outlined(SCREEN_WIDTH - str_px_width(run_buf) - 10, 10,
                                         run_buf, COLOR(0, 0, 0), COLOR(31, 31, 31));
                }

                // 10px green progress bar along the bottom: how far the front
                // wheel has travelled from the start flag toward the finish.
                if (state == STATE_GAME) {
                    int span = finish_x - start_x;
                    int front_px = get_pixel_coord(player_bike.nodes[1].x);
                    int fill = span > 0 ? ((front_px - start_x) * SCREEN_WIDTH) / span : 0;
                    if (fill < 0) fill = 0;
                    if (fill > SCREEN_WIDTH) fill = SCREEN_WIDTH;
                    draw_rect(0, SCREEN_HEIGHT - 5, SCREEN_WIDTH, 10, COLOR(3, 3, 3));
                    if (fill > 0) draw_rect(0, SCREEN_HEIGHT - 5, fill, 10, COLOR(4, 28, 4));
                }

                // Crash hit-flash overlays the whole scene for a few frames.
                if (state == STATE_GAME && crash_flash > 0) {
                    draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR(28, 0, 0));
                    draw_string_centered_outlined(76, "CRASH!", COLOR(31, 31, 31), COLOR(0, 0, 0));
                }

                if (state == STATE_FINISHED) {
                    draw_rect(44, 48, 152, 70, COLOR(31, 31, 31));
                    draw_rect(46, 50, 148, 66, COLOR(0, 0, 0));
                    draw_string_centered(56, "FINISHED!", COLOR(31, 31, 31));

                    char time_buf[10];
                    format_time(finish_time, time_buf);
                    draw_string_centered(70, time_buf, COLOR(0, 31, 0));

                    // Delta vs. the previous best (omitted on a first clear).
                    if (finish_has_delta) {
                        char dbuf[12];
                        format_delta(finish_delta, dbuf);
                        // Faster = negative = green; slower = positive = red.
                        color_t dcol = finish_delta <= 0 ? COLOR(0, 31, 0) : COLOR(31, 8, 8);
                        draw_string_centered(82, dbuf, dcol);
                    }

                    if (finish_unlocked)
                        draw_string_centered(98, "UNLOCKED NEXT!", COLOR(0, 31, 31));
                    else if (finish_new_best)
                        draw_string_centered(98, "NEW BEST TIME!", COLOR(31, 31, 0));
                }
            }
        }

        // All drawing above went to the off-screen back buffer (no beam race).
        // Wait for VBlank, then DMA the finished frame to VRAM — the copy
        // outruns the scan beam, so nothing tears. Skip the copy when we did
        // not redraw: VRAM already holds the last presented frame.
        vsync();
        if (redraw) present_frame();

        // Bring the freshly drawn screen up from black. The blocking fades ate
        // real time the fixed-timestep clock must not bank, or the first game
        // step would jump; resync the accumulator after the transition.
        if (do_fade) {
            fade_in();
            tm_prev = REG_TM0CNT_L;
            tm_accum = 0;
        }
        sound_tick();
        if (crash_flash > 0) crash_flash--;
    }

    return 0;
}
