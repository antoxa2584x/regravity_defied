#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>
#include "save.h"

// NDS storage backend: a flat save file on the SD card (flashcart DLDI on DS,
// internal SD on DSi) via libfat. The save struct is small and only written in
// menus / on finish, so reading and writing the whole blob each time is fine.

#define SAVE_PATH "regravity.sav"

static int fat_ready = -1;   // -1 = not yet probed

static int ensure_fat(void) {
    if (fat_ready < 0) fat_ready = fatInitDefault() ? 1 : 0;
    return fat_ready;
}

void save_backend_read(void* dst, int n) {
    // Default to all-zero so a missing/short file reads as "no save" (the magic
    // check in save_load then falls back to defaults).
    memset(dst, 0, n);
    if (!ensure_fat()) return;
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return;
    fread(dst, 1, n, f);
    fclose(f);
}

void save_backend_write(const void* src, int n) {
    if (!ensure_fat()) return;
    FILE* f = fopen(SAVE_PATH, "wb");
    if (!f) return;
    fwrite(src, 1, n, f);
    fclose(f);
}
