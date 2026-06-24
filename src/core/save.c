#include "save.h"

// "RGD5": bumped from "RGD4" when the customization colors were added to
// SaveData. A magic change clears any older save, so a struct that grew never
// reads garbage past the previous layout. ("RGD4" added sound_on over "RGD3".)
#define SAVE_MAGIC 0x52474435u

// Progress is ~5 KB now (1024-track flat arrays) and is only touched in menus
// and on finish — never in the per-frame hot path — so keep it out of the
// scarce, fast IWRAM (EWRAM_BSS on GBA). save_load() fills every byte from
// storage at boot before any read, so nothing depends on zero-initialization.
EWRAM_BSS SaveData g_save;

void save_load(void) {
    save_backend_read(&g_save, sizeof(g_save));
    if (g_save.magic != SAVE_MAGIC) {
        // No valid save: clear everything (only league 0 / track 0 unlocked).
        uint8_t* p = (uint8_t*)&g_save;
        for (unsigned i = 0; i < sizeof(g_save); i++) p[i] = 0;
        g_save.magic = SAVE_MAGIC;
        g_save.sound_on = 1;  // SFX default to ON, not the zeroed 0
        g_save.helmet_color = SAVE_DEFAULT_HELMET_COLOR;
        g_save.suit_color   = SAVE_DEFAULT_SUIT_COLOR;
        g_save.bike_color   = SAVE_DEFAULT_BIKE_COLOR;
        save_flush();
    }
}

void save_flush(void) {
    g_save.magic = SAVE_MAGIC;
    save_backend_write(&g_save, sizeof(g_save));
}

void save_reset(void) {
    for (int i = 0; i < MAX_TRACKS_TOTAL; i++) {
        g_save.completed[i] = 0;
        g_save.best[i] = 0;
    }
    for (int i = 0; i < NUM_LEAGUES; i++) g_save.last_track[i] = 0;
    save_flush();
}

void save_unlock_all(void) {
    for (int i = 0; i < MAX_TRACKS_TOTAL; i++) g_save.completed[i] = 1;
    save_flush();
}

int save_completed(int gidx) {
    if (gidx < 0 || gidx >= MAX_TRACKS_TOTAL) return 0;
    return g_save.completed[gidx];
}

uint32_t save_best(int gidx) {
    if (gidx < 0 || gidx >= MAX_TRACKS_TOTAL) return 0;
    return g_save.best[gidx];
}

int record_finish(int gidx, uint32_t time) {
    if (gidx < 0 || gidx >= MAX_TRACKS_TOTAL) return 0;
    g_save.completed[gidx] = 1;
    int is_best = (g_save.best[gidx] == 0 || time < g_save.best[gidx]);
    if (is_best) g_save.best[gidx] = time;
    save_flush();
    return is_best;
}

int save_last_track(int league) {
    if (league < 0 || league >= NUM_LEAGUES) return 0;
    return g_save.last_track[league];
}

void save_set_last_track(int league, int track) {
    if (league < 0 || league >= NUM_LEAGUES) return;
    if (track < 0) track = 0;
    if (g_save.last_track[league] == track) return;  // nothing changed; skip SRAM write
    g_save.last_track[league] = (uint16_t)track;
    save_flush();
}

int save_sound_on(void) {
    return g_save.sound_on;
}

void save_set_sound_on(int on) {
    on = on ? 1 : 0;
    if (g_save.sound_on == on) return;  // nothing changed; skip SRAM write
    g_save.sound_on = (uint8_t)on;
    save_flush();
}

int save_helmet_color(void) { return g_save.helmet_color; }
int save_suit_color(void)   { return g_save.suit_color; }
int save_bike_color(void)   { return g_save.bike_color; }

void save_set_colors(int helmet, int suit, int bike) {
    if (g_save.helmet_color == helmet &&
        g_save.suit_color == suit &&
        g_save.bike_color == bike) return;  // nothing changed; skip SRAM write
    g_save.helmet_color = (uint8_t)helmet;
    g_save.suit_color   = (uint8_t)suit;
    g_save.bike_color   = (uint8_t)bike;
    save_flush();
}
