#include "platform.h"
#include "graphics.h"
#include "level.h"
#include "physics.h"
#include "sound.h"
#include "save.h"
#include "gd_assets.h"
#include <stdlib.h>

// Game version string, normally baked in by the Makefile (-DGAME_VERSION).
#ifndef GAME_VERSION
#define GAME_VERSION "0.9"
#endif

// Title text. The 3DS build adds a "3D" tag to flag its stereoscopic top
// screen. The title is drawn in two pieces (green "Re" + black tail), so
// TITLE_TAIL carries the suffix and TITLE_FULL is used only for width math.
#if defined(PLATFORM_3DS)
#define TITLE_TAIL "Gravity Defied 3D"
#else
#define TITLE_TAIL "Gravity Defied"
#endif
#define TITLE_FULL "Re" TITLE_TAIL

enum State {
    STATE_INTRO,
    STATE_SPLASH,
    STATE_MENU_HARDNESS,
    STATE_MENU_TRACK,
    STATE_SETTINGS,
    STATE_CONFIRM_RESET,
    STATE_ABOUT,
    STATE_CUSTOMIZE,
    STATE_TRACK_VIEW,
    STATE_GAME,
    STATE_DEAD,       // crash hold: the wreck is shown for ~2s before respawning
    STATE_FINISHED
};

// Which buttons lean the bike. Chosen on the settings screen.
enum TiltMode { TILT_DPAD, TILT_SHOULDERS };
static int tilt_mode = TILT_DPAD;

// Sound on/off lives in SRAM (save_sound_on / save_set_sound_on), so the
// setting survives a power-off — unlike tilt_mode, which stays runtime-only.

// Settings screen cursor options.
enum { SET_TILT, SET_SOUND, SET_CUSTOMIZE, SET_RESET, SET_ABOUT, SET_COUNT };
static int settings_cursor = SET_TILT;

// Customize screen: which part the cursor is on, plus the live color indices
// (loaded from the save on entry, applied to draw_bike as they change, and
// persisted on exit). Indices are into the tinted sheet families / PALETTE.
enum { CUST_HELMET, CUST_BODY, CUST_BIKE, CUST_COUNT };
static int cust_cursor = CUST_HELMET;
static int cust_color[CUST_COUNT];
// Swatch + label for each palette color, in PALETTE order (see convert_assets.py).
static const color_t cust_swatch[CUSTOM_COLOR_COUNT] = {
    COLOR(31, 0, 0), COLOR(31, 28, 0), COLOR(0, 26, 0),
    COLOR(0, 24, 28), COLOR(3, 7, 31), COLOR(26, 0, 28),
};
static const char* cust_name[CUSTOM_COLOR_COUNT] = {
    "RED", "YELLOW", "GREEN", "CYAN", "BLUE", "PURPLE",
};
static const char* cust_part_name[CUST_COUNT] = { "HELMET", "BODY", "BIKE" };

// Push the live customize selection into the renderer (helmet, suit, bike).
static void cust_apply(void) {
    set_bike_colors(cust_color[CUST_HELMET], cust_color[CUST_BODY], cust_color[CUST_BIKE]);
}

Bike player_bike;

// Decorative bike that drives across the splash screen (see STATE_SPLASH). It is
// never simulated — init_bike gives it a fixed neutral pose and we just slide it
// across by feeding draw_bike a moving camera offset.
static Bike deco_bike;
static int  deco_x;     // screen x of the bike's frame node, in pixels
static int  deco_lane;  // screen y baseline for the current pass
static int  deco_step;  // horizontal speed, px per frame
static uint32_t deco_rng = 0x2545F491u;

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
// the menu stays readable. `phase` is a wall-clock-derived counter (one step per
// ~1/20 s) so the wave travels at a constant real-time speed regardless of how
// many frames the menu actually renders — like the blink, it never slows down.
#define CHK_CELL 24
static void draw_checker_bg(int phase) {
    // One period of a sine, ~6px amplitude (no libm on the GBA).
    static const signed char wave[16] =
        { 0, 3, 5, 6, 6, 6, 5, 3, 0, -3, -5, -6, -6, -6, -5, -3 };
    const color_t c = COLOR(58, 58, 58);   // faint grey ≈ translucent checker
    for (int col = -1; col * CHK_CELL < SCREEN_WIDTH; col++) {
        int x = col * CHK_CELL;
        int yoff = wave[(col * 2 + phase) & 15];   // neighbouring columns ripple
        for (int row = -1; row * CHK_CELL < SCREEN_HEIGHT + CHK_CELL; row++) {
            if (((col + row) & 1) == 0) continue;  // checkerboard: alternate cells
            draw_rect(x, row * CHK_CELL + yoff, CHK_CELL, CHK_CELL, c);
        }
    }
}

// Gentle vertical bob (±6px) for the floating splash title/mod text. Indexed off
// the wall clock (one full cycle ≈ 2s) so the float speed is steady regardless
// of frame rate.
static int float_offset(uint32_t clock_ticks) {
    static const signed char wave[16] =
        { 0, 3, 5, 6, 6, 6, 5, 3, 0, -3, -5, -6, -6, -6, -5, -3 };
    return wave[(clock_ticks / FLOAT_TICKS) & 15];
}

// Tiny LCG -> integer in [lo, hi]. Only used to give the splash bike a varied
// lane and speed on each pass, so statistical quality is irrelevant.
static int deco_rand(int lo, int hi) {
    deco_rng = deco_rng * 1103515245u + 12345u;
    return lo + (int)((deco_rng >> 16) % (uint32_t)(hi - lo + 1));
}

// (Re)launch the splash bike from just off the left edge with a fresh random
// lane and speed. Always travels left->right, the direction the bike faces.
static void deco_bike_launch(void) {
    deco_x    = -40;
    deco_lane = deco_rand(108, 134);
    deco_step = deco_rand(1, 3);
}

#if defined(DUAL_SCREEN)
// Bottom-screen detail card for the level-select menu: the highlighted track's
// name, best time, and a large preview of its shape — the counterpart to the
// compact detail pane on the top screen. Targets whichever canvas is active.
static void draw_track_detail_sub(const uint8_t* mrg, int league, int track) {
    int base = global_track_index(mrg, league, 0);
    int unlocked = track_unlocked(mrg, league, track);
    uint32_t best = save_best(base + track);
    const char* name = get_track_name(mrg, league, track);
    int cx = SCREEN_WIDTH / 2;

    // Track name, 2x scale, centered (scaled glyph width is 2x, so halving the
    // 1x width gives the left edge).
    draw_string_scaled(cx - str_px_width(name), 16, name, COLOR(0, 0, 0), 2);

    draw_string(cx - str_px_width("BEST TIME") / 2, 44, "BEST TIME", COLOR(8, 8, 8));
    char tb[12];
    if (best) format_time((int)best, tb); else str_cat(tb, "--:--.--");
    color_t tcol = best ? COLOR(0, 18, 0) : COLOR(15, 15, 15);
    draw_string_scaled(cx - str_px_width(tb), 56, tb, tcol, 2);

    // Large framed preview, centered. The inset is a light grey so the white box
    // stays visible against the white screen.
    const int bw = 184, bh = 84, bx = cx - bw / 2, by = 92;
    draw_rect(bx - 2, by - 2, bw + 4, bh + 4, COLOR(14, 14, 14));
    draw_rect(bx, by, bw, bh, COLOR(28, 28, 28));
    color_t trace = unlocked ? COLOR(0, 18, 0) : COLOR(18, 18, 18);
    draw_track_preview_flags(get_track_data(mrg, league, track), bx, by, bw, bh, trace, NULL);
    if (!unlocked)
        draw_string_centered_outlined(by + bh / 2 - 3, "LOCKED", COLOR(31, 8, 8), COLOR(31, 31, 31));
}

// Cached gameplay minimap. The track outline is static for a run, so it's traced
// once into g_mm_clean (rebuilt only when the track changes) and the per-frame
// work is just: copy the clean minimap back, drop the position marker, present.
// This keeps the bottom screen cheap enough to hold 60 fps during play.
static color_t g_mm_clean[SCREEN_WIDTH * SCREEN_HEIGHT] EWRAM_BSS;
static const uint8_t* g_mm_track;        // track g_mm_clean was built for
static TrackPreviewXform g_mm_xf;        // fit transform for the marker

static void build_minimap_cache(const uint8_t* cur_track) {
    gfx_clear(COLOR(31, 31, 31));
    draw_string_centered(6, "MAP", COLOR(8, 8, 8));
    const int bw = 224, bh = 156, bx = (SCREEN_WIDTH - bw) / 2, by = 22;
    draw_rect(bx - 2, by - 2, bw + 4, bh + 4, COLOR(14, 14, 14));
    draw_rect(bx, by, bw, bh, COLOR(27, 27, 27));
    // Track outline + start/finish markers, baked into the cache (static for the
    // run); g_mm_xf is kept so the live bike dot can be placed each frame.
    draw_track_preview_flags(cur_track, bx, by, bw, bh, COLOR(0, 18, 0), &g_mm_xf);

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) g_mm_clean[i] = g_subbuf[i];
    g_mm_track = cur_track;
}

static void draw_minimap_sub(const uint8_t* cur_track) {
    if (!cur_track) { gfx_clear(COLOR(31, 31, 31)); return; }
    if (cur_track != g_mm_track) build_minimap_cache(cur_track);
    // Restore the clean minimap, then stamp the bike's current position.
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) g_subbuf[i] = g_mm_clean[i];
    int mx, my;
    track_preview_map(&g_mm_xf, get_pixel_coord(player_bike.nodes[1].x),
                      get_pixel_coord(player_bike.nodes[1].y), &mx, &my);
    fill_circle(mx, my, 3, COLOR(0, 0, 0));     // dark halo
    fill_circle(mx, my, 2, COLOR(31, 0, 0));    // red position dot
}

// Compose the bottom (sub) screen for the current state, then present it. White
// everywhere except the "Remake by" intro (kept black); the start and league
// menus show the checkered-flag backdrop (matching the top screen) with the
// start screen's "PRESS START" prompt centered here; the level-select menu shows
// a track detail card; and during play it shows a minimap of progress. `chk_phase`
// animates the checker, `blink` drives the prompt.
static void draw_sub_screen(enum State state, const uint8_t* mrg, const uint8_t* cur_track,
                            int level_idx, int track_idx, int chk_phase, int blink) {
    gfx_target_sub();
    if (state == STATE_INTRO) {
        gfx_clear(COLOR(0, 0, 0));
    } else if (state == STATE_GAME || state == STATE_DEAD || state == STATE_FINISHED) {
        draw_minimap_sub(cur_track);
    } else {
        gfx_clear(COLOR(31, 31, 31));
        if (state == STATE_SPLASH || state == STATE_MENU_HARDNESS) {
            draw_checker_bg(chk_phase);
            if (state == STATE_SPLASH && blink)
                draw_string_centered_outlined(SCREEN_HEIGHT / 2 - 3, "PRESS START",
                                              COLOR(0, 0, 0), COLOR(31, 31, 31));
        } else if (state == STATE_MENU_TRACK) {
            draw_track_detail_sub(mrg, level_idx, track_idx);
        }
    }
    gfx_target_main();
    present_sub_frame();
}
#endif  // DUAL_SCREEN

// Per-frame HUD / finish-screen values, bundled so the 3DS stereo path can redraw
// the playfield once per eye without threading a long argument list each time.
typedef struct {
    int timer, attempts;       // run timer + attempt counter (HUD)
    int start_x, finish_x;     // progress-bar span (single-screen targets only)
    int crash_flash;           // red impact-flash countdown on the death screen
    int finish_time, finish_has_delta, finish_delta, finish_unlocked, finish_new_best;
} PlayHud;

// Draw the playfield (track, bike, flags) plus the gameplay HUD/overlays into the
// active back buffer. `eye_sign` is +1 / -1 for the left / right eye on the 3DS
// stereo path and 0 for a flat mono frame; it scales each layer's parallax via
// stereo_px(), which is a compile-time 0 on non-3DS targets, so this collapses to
// the original single-view render everywhere else.
static void render_gameplay(enum State state, const uint8_t* cur_track,
                            int cam_x, int cam_y, const PlayHud* h, int eye_sign) {
    // Per-layer horizontal parallax: the track's near (riding) edge sits just in
    // front of the screen plane, the bike floats further toward the viewer, and the
    // flags pop out the most. The track's far edge recedes inside draw_track
    // (level_set_stereo_eye), so the ribbon has real depth rather than being a card.
    int track_dx = eye_sign * stereo_px(STEREO_DEPTH_TRACK);
    int flag_dx  = eye_sign * stereo_px(STEREO_DEPTH_FLAG);
    int bike_dx  = eye_sign * stereo_px(STEREO_DEPTH_BIKE);

#if defined(PLATFORM_3DS)
    level_set_stereo_eye(eye_sign);
#endif
    draw_track(cur_track, cam_x - track_dx, cam_y);
    if (state == STATE_GAME || state == STATE_DEAD || state == STATE_FINISHED) {
        int bike_ox = SCREEN_WIDTH / 2 - cam_x;
        int bike_oy = SCREEN_HEIGHT / 2 + cam_y;
#if defined(DUAL_SCREEN)
        // The shadow lives on the track surface, so it takes the track's parallax
        // (not the bike's) — in 3D the bike then visibly floats above its shadow.
        draw_bike_shadow(&player_bike, bike_ox + track_dx, bike_oy);
#endif
        draw_bike(&player_bike, bike_ox + bike_dx, bike_oy);
    }
    // Flags last so one the rider is passing stays in front of the moto.
    draw_track_flags(cur_track, cam_x - flag_dx, cam_y);

    // HUD on top of the track so the timer stays readable. Outlined so it stays
    // legible over track lines and the rider. The per-eye render loop has set the
    // glyph parallax (gfx_set_text_parallax), so HUD text pops a touch toward the
    // viewer in 3D along with everything else, while staying gentle on the eyes.
    if (state == STATE_GAME) {
        char time_buf[10];
        format_time(h->timer, time_buf);
        draw_string_outlined(10, 10, time_buf, COLOR(0, 0, 0), COLOR(31, 31, 31));

        // Run counter, top-right ("x3" = on the third attempt).
        char run_buf[16];
        char* e = str_cat(run_buf, "x");
        str_num(e, h->attempts);
        draw_string_outlined(SCREEN_WIDTH - str_px_width(run_buf) - 10, 10,
                             run_buf, COLOR(0, 0, 0), COLOR(31, 31, 31));
    }

#if !defined(DUAL_SCREEN)
    // 10px green progress bar along the bottom: how far the front wheel has
    // travelled from the start flag toward the finish. (On dual-screen targets
    // this is replaced by the minimap on the second screen.)
    if (state == STATE_GAME) {
        int span = h->finish_x - h->start_x;
        int front_px = get_pixel_coord(player_bike.nodes[1].x);
        int fill = span > 0 ? ((front_px - h->start_x) * SCREEN_WIDTH) / span : 0;
        if (fill < 0) fill = 0;
        if (fill > SCREEN_WIDTH) fill = SCREEN_WIDTH;
        draw_rect(0, SCREEN_HEIGHT - 5, SCREEN_WIDTH, 10, COLOR(3, 3, 3));
        if (fill > 0) draw_rect(0, SCREEN_HEIGHT - 5, fill, 10, COLOR(4, 28, 4));
    }
#endif

    // Death screen: a brief red impact flash, then the wreck stays on screen with
    // a "CRASHED" message for the 2s hold (see STATE_DEAD).
    if (state == STATE_DEAD) {
        if (h->crash_flash > 0)
            draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR(28, 0, 0));
        draw_string_centered_outlined(SCREEN_HEIGHT / 2 - 12, "CRASHED",
                                      COLOR(31, 31, 31), COLOR(0, 0, 0));
        draw_string_centered_outlined(SCREEN_HEIGHT / 2 + 2, "A: RETRY   B: QUIT",
                                      COLOR(31, 28, 0), COLOR(0, 0, 0));
    }

    if (state == STATE_FINISHED) {
        // Centered modal (derived from the screen size so it sits in the middle on
        // every target).
        const int dw = 152, dh = 70;
        int dx = (SCREEN_WIDTH - dw) / 2;
        int dy = (SCREEN_HEIGHT - dh) / 2;
        draw_rect(dx, dy, dw, dh, COLOR(31, 31, 31));
        draw_rect(dx + 2, dy + 2, dw - 4, dh - 4, COLOR(0, 0, 0));
        draw_string_centered(dy + 8, "FINISHED!", COLOR(31, 31, 31));

        char time_buf[10];
        format_time(h->finish_time, time_buf);
        draw_string_centered(dy + 22, time_buf, COLOR(0, 31, 0));

        // Delta vs. the previous best (omitted on a first clear).
        if (h->finish_has_delta) {
            char dbuf[12];
            format_delta(h->finish_delta, dbuf);
            // Faster = negative = green; slower = positive = red.
            color_t dcol = h->finish_delta <= 0 ? COLOR(0, 31, 0) : COLOR(31, 8, 8);
            draw_string_centered(dy + 34, dbuf, dcol);
        }

        if (h->finish_unlocked)
            draw_string_centered(dy + 50, "UNLOCKED NEXT!", COLOR(0, 31, 31));
        else if (h->finish_new_best)
            draw_string_centered(dy + 50, "NEW BEST TIME!", COLOR(31, 31, 0));
    }
}

int main() {
    // Bring up the display, the free-running game-clock timer, and any memory
    // tuning for this target (see platform_<target>.c). The timer drives the
    // fixed-timestep loop: physics always advances at 60 Hz regardless of how
    // many frames the renderer actually presents.
    platform_init();
    sound_init();
    save_load();
    debug_init();

    const uint8_t* mrg = _binary_levels_mrg_start;

    // Pose the decorative splash bike once on the first track; it is only ever
    // translated after this, never simulated.
    physics_set_league(0);
    init_bike(&deco_bike, get_track_data(mrg, 0, 0));
    deco_bike_launch();

    // Apply the saved customization colors so the splash bike (and everything
    // after) is drawn in the player's chosen colors from the first frame.
    set_bike_colors(save_helmet_color(), save_suit_color(), save_bike_color());

    int level_idx = 0;
    int track_idx = 0;
    int cam_x = 0, cam_y = 0;
    int frame = 0;
    enum State state = STATE_INTRO;
    const uint8_t* cur_track = NULL; // cached; set when entering STATE_GAME
    
    uint16_t prev_keys = 0;
    uint16_t held_dpad = 0;   // d-pad bits held last frame, for menu auto-repeat
    int hold_frames = 0;      // frames the current d-pad direction has been held
    int cheat_idx = 0;        // progress through the About-screen unlock code
    int cheat_done = 0;       // show the "ALL UNLOCKED" banner after the code lands
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
    uint32_t crash_deadline = 0; // wall-clock tick at which the death hold ends
    int prev_state = -1;   // force a redraw on the first frame
    int prev_blink = -1;

    uint16_t tm_prev = platform_timer();  // last timer sample
    int tm_accum = 0;                 // unspent time, in timer ticks
    uint32_t clock_ticks = 0;         // wall-clock tick count (fps-independent)

    while (1) {
        uint16_t keys = platform_keys();
        uint16_t keys_pressed = keys & ~prev_keys;
        prev_keys = keys;

        // D-pad auto-repeat for menu navigation: a freshly pressed direction
        // fires once, then holding it repeats after a short delay. Lets the
        // league/level lists scroll quickly when a direction is held instead of
        // stepping one row per press. Repeats fire only while a single, steady
        // direction is held; changing or releasing the d-pad resets the timer.
        #define MENU_REPEAT_DELAY 14  // frames held before auto-repeat begins
        #define MENU_REPEAT_RATE   3  // frames between repeats while held
        uint16_t dpad = keys & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);
        uint16_t repeat_fire = 0;
        if (dpad != held_dpad) {
            held_dpad = dpad;
            hold_frames = 0;
        } else if (dpad) {
            hold_frames++;
            if (hold_frames >= MENU_REPEAT_DELAY &&
                (hold_frames - MENU_REPEAT_DELAY) % MENU_REPEAT_RATE == 0) {
                repeat_fire = dpad;
            }
        }
        uint16_t keys_repeat = keys_pressed | repeat_fire;

        // Fixed-timestep clock: how many 60 Hz sim steps are owed for the real
        // time elapsed since last loop. At 30 fps render this is 2 per frame,
        // keeping the simulation at full speed. Clamp to avoid a catch-up spiral
        // after a long stall (e.g. level load).
        uint16_t tm_now = platform_timer();
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
            // Drive the decorative bike across; relaunch with a new random lane
            // and speed once it leaves the right edge.
            deco_x += deco_step;
            if (deco_x > SCREEN_WIDTH + 40) deco_bike_launch();
            if (keys_pressed & (KEY_START | KEY_A | KEY_B)) {
                state = STATE_MENU_HARDNESS;
            }
        } else if (state == STATE_MENU_HARDNESS) {
            if (keys_repeat & KEY_UP) {
                if (level_idx > 0) level_idx--;
            }
            if (keys_repeat & KEY_DOWN) {
                if (level_idx < NUM_LEAGUES - 1) level_idx++;
            }
            if (keys_pressed & (KEY_START | KEY_A)) {
                if (league_unlocked(mrg, level_idx)) {
                    state = STATE_MENU_TRACK;
                    // Restore the saved cursor, clamped in case this track pack
                    // has fewer tracks in the league than the one that saved it.
                    int n = level_track_count(mrg, level_idx);
                    track_idx = save_last_track(level_idx);
                    if (track_idx >= n) track_idx = n > 0 ? n - 1 : 0;
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
            } else if (settings_cursor == SET_SOUND) {
                if (keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_A)) {
                    save_set_sound_on(!save_sound_on());
                }
            } else if (settings_cursor == SET_CUSTOMIZE) {
                if (keys_pressed & KEY_A) {
                    state = STATE_CUSTOMIZE;
                    cust_cursor = CUST_HELMET;
                    cust_color[CUST_HELMET] = save_helmet_color();
                    cust_color[CUST_BODY]   = save_suit_color();
                    cust_color[CUST_BIKE]   = save_bike_color();
                }
            } else if (settings_cursor == SET_RESET) {
                if (keys_pressed & KEY_A) state = STATE_CONFIRM_RESET;
            } else if (settings_cursor == SET_ABOUT) {
                if (keys_pressed & KEY_A) {
                    state = STATE_ABOUT;
                    cheat_idx = 0;   // fresh code entry each visit
                    cheat_done = 0;
                }
            }
            if (keys_pressed & (KEY_B | KEY_SELECT)) {
                state = STATE_MENU_HARDNESS;
            }
        } else if (state == STATE_CUSTOMIZE) {
            // UP/DOWN pick the part; LEFT/RIGHT cycle its color (wrapping), with
            // the bike preview updating live. B/SELECT persists and returns.
            if (keys_pressed & KEY_UP)   cust_cursor = (cust_cursor + CUST_COUNT - 1) % CUST_COUNT;
            if (keys_pressed & KEY_DOWN) cust_cursor = (cust_cursor + 1) % CUST_COUNT;
            if (keys_pressed & KEY_LEFT)
                cust_color[cust_cursor] = (cust_color[cust_cursor] + CUSTOM_COLOR_COUNT - 1) % CUSTOM_COLOR_COUNT;
            if (keys_pressed & KEY_RIGHT)
                cust_color[cust_cursor] = (cust_color[cust_cursor] + 1) % CUSTOM_COLOR_COUNT;
            if (keys_pressed & (KEY_LEFT | KEY_RIGHT)) cust_apply();
            if (keys_pressed & (KEY_B | KEY_SELECT)) {
                save_set_colors(cust_color[CUST_HELMET], cust_color[CUST_BODY], cust_color[CUST_BIKE]);
                state = STATE_SETTINGS;
            }
        } else if (state == STATE_ABOUT) {
            // Konami-style unlock code. Each correct key advances the sequence;
            // a wrong d-pad key re-syncs from the start, and B/SELECT/A/START
            // exit to settings UNLESS they're the next expected key (the code
            // ends in B, A, START, so those must advance rather than back out).
            static const uint16_t code[] = {
                KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
                KEY_LEFT, KEY_RIGHT, KEY_B, KEY_A, KEY_START
            };
            const int code_len = (int)(sizeof(code) / sizeof(code[0]));
            if (keys_pressed) {
                if (keys_pressed & code[cheat_idx]) {
                    if (++cheat_idx >= code_len) {
                        save_unlock_all();
                        cheat_done = 1;
                        cheat_idx = 0;
                    }
                } else if (keys_pressed & (KEY_B | KEY_SELECT | KEY_A | KEY_START)) {
                    cheat_idx = 0;
                    state = STATE_SETTINGS;
                } else {
                    // Wrong d-pad key: restart, allowing it to be a fresh first key.
                    cheat_idx = (keys_pressed & code[0]) ? 1 : 0;
                }
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
            if (keys_repeat & KEY_UP) {
                if (track_idx > 0) track_idx--;
            }
            if (keys_repeat & KEY_DOWN) {
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
                save_set_last_track(level_idx, track_idx);  // persist for next level-screen visit
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
                    if (save_sound_on()) sound_play_crash();
                    // Hold on the wreck for 2s (see STATE_DEAD) before respawning,
                    // rather than restarting instantly. Keep the crash pose on
                    // screen; the respawn happens when the hold expires.
                    crash_flash = 6;   // ~0.1s red hit-flash on impact
                    crash_deadline = clock_ticks + 2u * CLOCK_HZ;
                    state = STATE_DEAD;
                    break;
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
        } else if (state == STATE_DEAD) {
            // Hold on the wreck for ~2s, then respawn for the next attempt.
            // START/A skips the wait; SELECT/B bails to the track menu.
            if (keys_pressed & (KEY_SELECT | KEY_B)) {
                state = STATE_MENU_TRACK;
            } else if (clock_ticks >= crash_deadline || (keys_pressed & (KEY_START | KEY_A))) {
                init_bike(&player_bike, cur_track);
                update_physics(&player_bike, cur_track, 0);
                timer = 0;
                timer_started = 0;
                attempts++;
                crash_flash = 0;
                tm_accum = 0;   // don't bank the hold time into the first sim step
                state = STATE_GAME;
            }
            cam_x = get_pixel_coord(player_bike.x);
            cam_y = get_pixel_coord(player_bike.y);
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
        int blink = ((clock_ticks / BLINK_TICKS) & 1) == 0;
        int blinking_state = (state == STATE_SPLASH ||
                              state == STATE_MENU_HARDNESS ||
                              state == STATE_MENU_TRACK ||
                              state == STATE_SETTINGS ||
                              state == STATE_CUSTOMIZE);
        // Only redraw when something visible can change: the game animates every
        // frame, a blinking screen on each toggle, and any screen on entry or a
        // key press. Static screens (intro, finished) are drawn once and skip
        // both the redraw and the back-buffer->VRAM copy below, so an idle menu
        // costs almost nothing.
        // A screen change (not the very first frame) gets a fade transition.
        // The finish modal is excluded: it should pop up instantly over the
        // frozen track rather than fading the gameplay away.
        int transition = (int)state != prev_state;
        // No fade into/out of the death hold either — it pops over the frozen
        // track like the finish modal, and the respawn shouldn't fade.
        int do_fade = transition && prev_state >= 0 && state != STATE_FINISHED
                   && state != STATE_DEAD && prev_state != STATE_DEAD;
        int redraw = (state == STATE_GAME)
                   || (state == STATE_DEAD)            // animate the crash flash/message
                   || (state == STATE_MENU_HARDNESS)   // animated checker backdrop
                   || (state == STATE_SPLASH)          // animated checker + floating title
                   || transition
                   || keys_pressed
                   || (blinking_state && blink != prev_blink);
        prev_state = state;
        prev_blink = blink;

        // Fade the outgoing screen (still in VRAM) to black before drawing the
        // new one over it; the matching fade-in runs after present_frame below.
        if (do_fade) fade_out();

        // Set when the gameplay stereo path has already presented both top-screen
        // eyes itself, so the mono present below is skipped (3DS only; always 0
        // elsewhere, leaving the original single present in place).
        int top_presented = 0;

        if (redraw) {
            int eye_sign = 0;   // 3DS stereo: +1 left / -1 right; 0 = flat (all other targets)
#if defined(PLATFORM_3DS)
            // Autostereoscopic top screen: with the 3D slider engaged, draw the
            // whole top screen once per eye (eye_n == 2) so the track ribbon, the
            // bike/flags and every glyph all separate in depth; with the slider off
            // it is a single flat pass, exactly like the mono targets below.
            int eye_n = stereo_active() ? 2 : 1;
            for (int eye_i = 0; eye_i < eye_n; eye_i++) {
                eye_sign = eye_n == 1 ? 0 : (eye_i ? -1 : +1);
                gfx_set_text_parallax(eye_sign * stereo_px(STEREO_DEPTH_TEXT));
#endif
            clear_screen(state == STATE_INTRO ? COLOR(0, 0, 0) : COLOR(31, 31, 31));

            if (state == STATE_INTRO) {
                draw_string_centered(76, "Remake by rgaming.com.ua", COLOR(0, 31, 0));
            } else if (state == STATE_SPLASH) {
                // Animated checkered-flag backdrop, matching the league select.
                draw_checker_bg(clock_ticks / CHECKER_TICKS);
                // Decorative bike driving across, slightly bobbing. draw_bike
                // treats (ox, oy) as a camera offset, so we offset by the frame
                // node's own pixel coords to land it at (deco_x, lane + bob).
                int bx0 = get_pixel_coord(deco_bike.nodes[0].x);
                int by0 = get_pixel_coord(deco_bike.nodes[0].y);
                int deco_y = deco_lane + float_offset(clock_ticks) / 3;
                draw_bike(&deco_bike, deco_x - bx0, deco_y + by0);
                // "ReGravity Defied" title + mod name, centered, bobbing up and
                // down together. Same styling as the league select title: "Re"
                // green, the rest black, 2x scale, white outline for legibility.
                int fy = float_offset(clock_ticks);
                int title_x = (SCREEN_WIDTH - str_px_width(TITLE_FULL) * 2) / 2;
                draw_string_scaled_outlined(title_x, 56 + fy, "Re", COLOR(0, 31, 0), COLOR(31, 31, 31), 2);
                draw_string_scaled_outlined(title_x + str_px_width("Re") * 2, 56 + fy, TITLE_TAIL, COLOR(0, 0, 0), COLOR(31, 31, 31), 2);
                if (MOD_NAME[0]) {
                    char mbuf[24];
                    char* me = str_cat(mbuf, MOD_NAME);
                    str_cat(me, " MOD");
                    draw_string_centered_outlined(78 + fy, mbuf, COLOR(0, 20, 0), COLOR(31, 31, 31));
                }
#if !defined(DUAL_SCREEN)
                // On dual-screen targets the prompt lives centered on the bottom screen instead.
                if (blink) draw_string_centered_outlined(120, "PRESS START", COLOR(0, 0, 0), COLOR(31, 31, 31));
#endif
            } else if (state == STATE_MENU_HARDNESS) {
                static const char* league_names[3] = { "100cc", "175cc", "220cc" };
                // Wall-clock phase (16384 Hz / 819 ≈ 20 steps/s) so the flag waves
                // at the same speed whether the menu renders at 60 or fewer fps.
                draw_checker_bg(clock_ticks / CHECKER_TICKS);   // animated semi-transparent checkered flag
                // Title "ReGravity Defied": "Re" green, the rest black, centered. 2x scale.
                // White 1px outline keeps it readable over the checkered backdrop.
                int title_x = (SCREEN_WIDTH - str_px_width(TITLE_FULL) * 2) / 2;
                draw_string_scaled_outlined(title_x, 18, "Re", COLOR(0, 31, 0), COLOR(31, 31, 31), 2);
                draw_string_scaled_outlined(title_x + str_px_width("Re") * 2, 18, TITLE_TAIL, COLOR(0, 0, 0), COLOR(31, 31, 31), 2);
                // Mod label ("<mrg name> MOD") under the title, from the embedded levels file.
                if (MOD_NAME[0]) {
                    char mbuf[24];
                    char* me = str_cat(mbuf, MOD_NAME);
                    str_cat(me, " MOD");
                    draw_string_centered_outlined(38, mbuf, COLOR(0, 20, 0), COLOR(31, 31, 31));
                }
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
                draw_menu_row(50, tbuf, tcol, settings_cursor == SET_TILT, blink);

                char sbuf[16];
                char* se = str_cat(sbuf, "SOUND: ");
                str_cat(se, save_sound_on() ? "ON" : "OFF");
                color_t scol = (settings_cursor == SET_SOUND) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                draw_menu_row(66, sbuf, scol, settings_cursor == SET_SOUND, blink);

                color_t ccol = (settings_cursor == SET_CUSTOMIZE) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                draw_menu_row(82, "CUSTOMIZE", ccol, settings_cursor == SET_CUSTOMIZE, blink);

                color_t rcol = (settings_cursor == SET_RESET) ? COLOR(31, 0, 0) : COLOR(15, 8, 8);
                draw_menu_row(98, "RESET PROGRESS", rcol, settings_cursor == SET_RESET, blink);

                color_t acol = (settings_cursor == SET_ABOUT) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                draw_menu_row(114, "ABOUT", acol, settings_cursor == SET_ABOUT, blink);

                draw_string_centered(134, "A: SELECT   UP/DOWN", COLOR(10, 10, 10));
                draw_string_centered(146, "B: BACK", COLOR(10, 10, 10));
            } else if (state == STATE_ABOUT) {
                int tx = (SCREEN_WIDTH - str_px_width(TITLE_FULL) * 2) / 2;
                draw_string_scaled(tx, 14, "Re", COLOR(0, 31, 0), 2);
                draw_string_scaled(tx + str_px_width("Re") * 2, 14, TITLE_TAIL, COLOR(0, 0, 0), 2);
                draw_string_centered(32, "VERSION " GAME_VERSION, COLOR(0, 22, 0));
                draw_string_centered(44, "OPEN SOURCE PORT OF", COLOR(10, 10, 10));
                draw_string_centered(56, "GRAVITY DEFIED", COLOR(0, 0, 0));
                draw_string_centered(86, "GITHUB.COM/ANTOXA2584X", COLOR(0, 22, 0));
                draw_string_centered(98, "/REGRAVITY_DEFIED_GBA", COLOR(0, 22, 0));
                if (cheat_done)
                    draw_string_centered(120, "ALL LEVELS UNLOCKED!", COLOR(0, 31, 0));
                draw_string_centered(140, "B: BACK", COLOR(10, 10, 10));
            } else if (state == STATE_CUSTOMIZE) {
                draw_string_centered(12, "CUSTOMIZE", COLOR(0, 0, 0));

                // Live preview: the neutral splash bike, centered, drawn in the
                // colors currently selected (cust_apply has pushed them already).
                // draw_bike treats (ox, oy) as a camera offset, so offset by the
                // frame node's own pixel coords to land it at (cx, cy).
                int bx0 = get_pixel_coord(deco_bike.nodes[0].x);
                int by0 = get_pixel_coord(deco_bike.nodes[0].y);
                draw_bike(&deco_bike, SCREEN_WIDTH / 2 - bx0, 60 + by0);

                for (int i = 0; i < CUST_COUNT; i++) {
                    int y = 92 + i * 16;
                    int ci = cust_color[i];
                    color_t tc = (cust_cursor == i) ? COLOR(0, 31, 0) : COLOR(10, 10, 10);
                    if (cust_cursor == i && blink) draw_string(26, y, ">", tc);
                    draw_string(38, y, cust_part_name[i], tc);
                    draw_string(110, y, cust_name[ci], tc);
                    // Outlined swatch of the selected color.
                    draw_rect(186, y - 1, 22, 9, COLOR(0, 0, 0));
                    draw_rect(187, y, 20, 7, cust_swatch[ci]);
                }

                draw_string_centered(146, "L/R: COLOR  U/D: PART  B: BACK", COLOR(10, 10, 10));
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
#if !defined(DUAL_SCREEN)
                draw_line(120, 22, 120, 140, COLOR(18, 18, 18));   // rail/pane divider
#endif

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
#if !defined(DUAL_SCREEN)
                const char* sel_name = "";   // captured for the detail pane
#endif
                for (int t = 0; t < (int)count; t++) {
                    track_info += 4;
                    const char* name = (const char*)track_info;
#if !defined(DUAL_SCREEN)
                    if (t == track_idx) sel_name = name;
#endif

                    // Window of 7 names around the cursor.
                    if (t >= track_idx - 3 && t <= track_idx + 3) {
                        int unlocked = track_unlocked(mrg, level_idx, t);
                        color_t color = !unlocked ? COLOR(15, 15, 15)
                                      : (t == track_idx) ? COLOR(0, 31, 0) : COLOR(8, 8, 8);
                        int y_pos = 32 + (t - (track_idx - 3)) * 14;
#if defined(DUAL_SCREEN)
                        // Dual-screen: the detail card lives on the bottom screen,
                        // so the top screen shows just the track list, centered.
                        int nx = (SCREEN_WIDTH - str_px_width(name)) / 2;
                        if (t == track_idx && blink) draw_string(nx - 12, y_pos, ">", color);
                        draw_string(nx, y_pos, name, color);
                        if (save_completed(base + t))            // tick = cleared
                            draw_string(nx + str_px_width(name) + 4, y_pos, "*", COLOR(0, 28, 0));
#else
                        if (t == track_idx && blink) draw_string(6, y_pos, ">", color);
                        draw_string(16, y_pos, name, color);
                        if (save_completed(base + t))            // tick = cleared
                            draw_string(16 + str_px_width(name) + 4, y_pos, "*", COLOR(0, 28, 0));
#endif
                    }

                    while (*track_info) track_info++;
                    track_info++;
                }

#if !defined(DUAL_SCREEN)
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
                    draw_track_preview_flags(get_track_data(mrg, level_idx, track_idx), bx, by, bw, bh, trace, NULL);
                    if (!sel_unlocked) {
                        const char* lk = "LOCKED";
                        draw_string_outlined(pcx - str_px_width(lk) / 2, by + bh / 2 - 3,
                                             lk, COLOR(31, 8, 8), COLOR(31, 31, 31));
                    }
                }
#endif

                draw_string_centered(150, "A: START   B: BACK", COLOR(10, 10, 10));
            } else if (state == STATE_GAME || state == STATE_TRACK_VIEW
                       || state == STATE_DEAD || state == STATE_FINISHED) {
                PlayHud hud = { timer, attempts, start_x, finish_x, crash_flash,
                                finish_time, finish_has_delta, finish_delta,
                                finish_unlocked, finish_new_best };
                // The enclosing per-eye loop (3DS) supplies eye_sign and re-clears
                // for each eye; the layer parallax happens inside render_gameplay.
                render_gameplay(state, cur_track, cam_x, cam_y, &hud, eye_sign);
            }
#if defined(PLATFORM_3DS)
            // Push this eye's finished frame to its own top framebuffer. With the
            // slider off (eye_n == 1) fall through to the mono present below instead.
            if (eye_n == 2) { present_frame_top(eye_i); top_presented = 1; }
            }   // per-eye loop
            gfx_set_text_parallax(0);   // bottom screen and the next frame draw flat
#endif
        }

        // All drawing above went to the off-screen back buffer (no beam race).
        // Wait for VBlank, then DMA the finished frame to VRAM — the copy
        // outruns the scan beam, so nothing tears. Skip the copy when we did
        // not redraw: VRAM already holds the last presented frame.
        platform_vsync();
        if (redraw && !top_presented) present_frame();
#if defined(DUAL_SCREEN)
        // Compose and present the bottom screen from the same redraw decision as
        // the top, so it stays in step (and idle screens stay cheap).
        // The minimap doesn't need 60 Hz; refresh the bottom screen every 3rd
        // gameplay frame so the CPU budget stays with the playfield. Menus and
        // the finish screen present on their (already gated) redraws.
        int sub_throttled = (state == STATE_GAME || state == STATE_DEAD) && (frame % 3 != 0);
        if (redraw && !sub_throttled)
            draw_sub_screen(state, mrg, cur_track, level_idx, track_idx,
                            clock_ticks / CHECKER_TICKS, blink);
#endif

        // Bring the freshly drawn screen up from black. The blocking fades ate
        // real time the fixed-timestep clock must not bank, or the first game
        // step would jump; resync the accumulator after the transition.
        if (do_fade) {
            fade_in();
            tm_prev = platform_timer();
            tm_accum = 0;
        }
        sound_tick();
        if (crash_flash > 0) crash_flash--;
    }

    return 0;
}
