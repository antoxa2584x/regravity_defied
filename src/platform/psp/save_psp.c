#include <stdio.h>
#include <string.h>
#include "save.h"

// Sony PSP storage backend: a flat save file on the Memory Stick, written with
// plain stdio (pspsdk's libc routes "ms0:/" to the stick). The save struct is
// small and only written in menus / on finish, so reading and writing the whole
// blob each time is fine — this mirrors the DS libfat and 3DS sdmc backends.
//
// The file lives at a fixed absolute path so it is found regardless of where the
// EBOOT was launched from; PSP/SAVEDATA is the conventional home for save data.

#define SAVE_PATH "ms0:/PSP/SAVEDATA/regravity_defied.sav"

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
