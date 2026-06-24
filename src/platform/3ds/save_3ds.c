#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "save.h"

// Nintendo 3DS storage backend: a flat save file on the SD card. libctru's
// default init mounts the "sdmc" device, so plain stdio reaches the card root.
// The save struct is small and only written in menus / on finish, so reading and
// writing the whole blob each time is fine (mirrors the GBA SRAM / DS libfat
// backends).

#define SAVE_PATH "sdmc:/regravity_defied.sav"

void save_backend_read(void* dst, int n) {
    // Default to all-zero so a missing/short file reads as "no save" (the magic
    // check in save_load then falls back to defaults).
    memset(dst, 0, n);
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return;
    fread(dst, 1, n, f);
    fclose(f);
}

void save_backend_write(const void* src, int n) {
    FILE* f = fopen(SAVE_PATH, "wb");
    if (!f) return;
    fwrite(src, 1, n, f);
    fclose(f);
}
