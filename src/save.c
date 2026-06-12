#include "save.h"

// SRAM lives at 0x0E000000 and must be accessed one byte at a time.
#define SRAM ((volatile uint8_t*)0x0E000000)
#define SAVE_MAGIC 0x52474431u  // "RGD1"

// Emulators/flashcarts enable 32 KB SRAM when this marker is present in the ROM.
__attribute__((used)) static const char sram_sig[] = "SRAM_V113";

SaveData g_save;

static void sram_read(void* dst, int n) {
    uint8_t* d = dst;
    for (int i = 0; i < n; i++) d[i] = SRAM[i];
}

static void sram_write(const void* src, int n) {
    const uint8_t* s = src;
    for (int i = 0; i < n; i++) SRAM[i] = s[i];
}

void save_load(void) {
    sram_read(&g_save, sizeof(g_save));
    if (g_save.magic != SAVE_MAGIC) {
        // No valid save: clear everything (only league 0 / track 0 unlocked).
        uint8_t* p = (uint8_t*)&g_save;
        for (unsigned i = 0; i < sizeof(g_save); i++) p[i] = 0;
        g_save.magic = SAVE_MAGIC;
        save_flush();
    }
}

void save_flush(void) {
    g_save.magic = SAVE_MAGIC;
    sram_write(&g_save, sizeof(g_save));
}

void save_reset(void) {
    for (int l = 0; l < NUM_LEAGUES; l++) {
        for (int t = 0; t < MAX_TRACKS; t++) {
            g_save.completed[l][t] = 0;
            g_save.best[l][t] = 0;
        }
    }
    save_flush();
}

int league_unlocked(int league, int prev_track_count) {
    if (league <= 0) return 1;
    for (int t = 0; t < prev_track_count; t++) {
        if (!g_save.completed[league - 1][t]) return 0;
    }
    return 1;
}

int track_unlocked(int league, int track, int prev_track_count) {
    if (!league_unlocked(league, prev_track_count)) return 0;
    if (track == 0) return 1;
    return g_save.completed[league][track - 1];
}

int record_finish(int league, int track, uint32_t time) {
    if (league < 0 || league >= NUM_LEAGUES || track < 0 || track >= MAX_TRACKS)
        return 0;
    g_save.completed[league][track] = 1;
    int is_best = (g_save.best[league][track] == 0 || time < g_save.best[league][track]);
    if (is_best) g_save.best[league][track] = time;
    save_flush();
    return is_best;
}
