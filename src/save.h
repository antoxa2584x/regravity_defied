#ifndef SAVE_H
#define SAVE_H

#include "gba.h"

// Number of engine leagues. Fixed by the levels.mrg layout (it has no league
// count header), but the tracks-per-league split is read from the file and may
// be anything, so progress is stored by a flat global track index instead of a
// fixed per-league grid.
#define NUM_LEAGUES 3
// Upper bound on the total number of tracks across all leagues. Sized to cover
// any track pack up to 1000 tracks. completed[] + best[] = 1024*5 = 5120 bytes,
// well within the 32 KB SRAM. Bump SAVE_MAGIC in save.c if this changes.
#define MAX_TRACKS_TOTAL 1024

// Persistent progress, mirrored to battery-backed SRAM. Indexed by global track
// index (sum of the track counts of all preceding leagues, plus the in-league
// track index) — see global_track_index() in level.c.
typedef struct {
    uint32_t magic;                          // validity marker
    uint8_t  completed[MAX_TRACKS_TOTAL];    // 1 once a track is finished
    uint32_t best[MAX_TRACKS_TOTAL];         // best time in frames (0 = none)
    uint16_t last_track[NUM_LEAGUES];        // last track opened per league
} SaveData;

extern SaveData g_save;

// Load progress from SRAM into g_save; reset to defaults if no valid save.
void save_load(void);
// Write g_save back to SRAM.
void save_flush(void);
// Erase all progress and best times (then persist).
void save_reset(void);

// Bounds-checked accessors keyed by global track index. Out-of-range indices
// read as "not completed" / "no best time".
int      save_completed(int gidx);
uint32_t save_best(int gidx);
// Record a finish: mark completed, keep the better time, persist. Returns 1 if
// this run set a new best time.
int record_finish(int gidx, uint32_t time);

// Last track opened in a league (cursor position the level screen restores to).
// Out-of-range leagues read as track 0. Setter persists immediately.
int  save_last_track(int league);
void save_set_last_track(int league, int track);

#endif // SAVE_H
