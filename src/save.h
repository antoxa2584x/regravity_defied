#ifndef SAVE_H
#define SAVE_H

#include "gba.h"

#define NUM_LEAGUES 3
#define MAX_TRACKS  10

// Persistent progress, mirrored to battery-backed SRAM.
typedef struct {
    uint32_t magic;                              // validity marker
    uint8_t  completed[NUM_LEAGUES][MAX_TRACKS]; // 1 once a track is finished
    uint32_t best[NUM_LEAGUES][MAX_TRACKS];      // best time in frames (0 = none)
} SaveData;

extern SaveData g_save;

// Load progress from SRAM into g_save; reset to defaults if no valid save.
void save_load(void);
// Write g_save back to SRAM.
void save_flush(void);
// Erase all progress and best times (then persist).
void save_reset(void);

// A league is unlocked if it is the first or the previous league is fully done.
int league_unlocked(int league, int prev_track_count);
// A track is unlocked if its league is and the previous track is completed.
int track_unlocked(int league, int track, int prev_track_count);
// Record a finish: mark completed, keep the better time, persist. Returns 1 if
// this run set a new best time.
int record_finish(int league, int track, uint32_t time);

#endif // SAVE_H
