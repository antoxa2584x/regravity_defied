// Host-side harness: runs the real physics + draw_bike into a fake VRAM buffer
// and dumps raw frames so we can inspect the sprite rendering without a GBA
// emulator. Build via tools/render.sh.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "gba.h"
#include "graphics.h"
#include "level.h"
#include "physics.h"

uint16_t g_vram_buf[SCREEN_WIDTH * SCREEN_HEIGHT];

extern Bike player_bike_dummy;
Bike bk;

static uint8_t* read_file(const char* path, long* n) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* b = malloc(*n);
    if (fread(b, 1, *n, f) != (size_t)*n) { exit(1); }
    fclose(f);
    return b;
}

static void dump_ppm(const char* path) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint16_t c = g_vram_buf[i];
        unsigned char r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    int league = argc > 1 ? atoi(argv[1]) : 0;
    int steps  = argc > 2 ? atoi(argv[2]) : 0;
    const char* out = argc > 3 ? argv[3] : "/tmp/frame.ppm";

    long n;
    uint8_t* mrg = read_file("levels.mrg", &n);
    const uint8_t* track = get_track_data(mrg, league, 0);

    physics_set_league(league);
    init_bike(&bk, track);
    update_physics(&bk, track, 0);
    for (int i = 0; i < steps; i++)
        update_physics(&bk, track, KEY_A | KEY_RIGHT);

    // White background
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) g_vram_buf[i] = COLOR(31, 31, 31);

    int cam_x = get_pixel_coord(bk.x);
    int cam_y = get_pixel_coord(bk.y);
    draw_track(track, cam_x, cam_y);
    draw_bike(&bk, SCREEN_WIDTH / 2 - cam_x, SCREEN_HEIGHT / 2 + cam_y);
    dump_ppm(out);
    printf("league=%d steps=%d -> %s\n", league, steps, out);
    return 0;
}
