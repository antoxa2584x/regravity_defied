#ifndef PHYSICS_H
#define PHYSICS_H

#include "platform.h"

#define FP_SHIFT 16
#define TO_FP(x) ((int32_t)((x) << FP_SHIFT))
#define FROM_FP(x) ((int32_t)((x) >> FP_SHIFT))

typedef struct {
    int32_t x, y;    // Position (fixed point 16.16)
    int32_t old_x, old_y; // Previous position for Verlet
    int32_t radius;
    int grounded;
    int32_t nx, ny;  // Surface normal (fixed point 16.16)
} PhysicsPoint;

typedef struct {
    PhysicsPoint nodes[6]; // 0=Frame, 1=FrontWheel, 2=RearWheel, 3=Fork, 4=RearMount, 5=Rider
    int32_t x, y;    // View center (approximate)
    int crash;
} Bike;

#define GRAVITY 100
#define ACCEL 40
#define LEAN_FORCE 12
#define FRICTION 252 
#define WHEEL_DIST 14
#define SURFACE_FRICTION 240

void init_bike(Bike* b, const uint8_t* track_data);
void update_physics(Bike* b, const uint8_t* track_data, uint16_t keys);
void draw_bike(Bike* b, int ox, int oy);

// Select engine league before init_bike: 0=100cc, 1=175cc, 2=220cc.
void physics_set_league(int league);

// Set the rider/bike customization colors used by draw_bike. Each argument is
// an index into the per-part tinted sheet families (0..CUSTOM_COLOR_COUNT-1);
// out-of-range values are ignored. Persisted via the save (see main.c).
void set_bike_colors(int helmet, int suit, int bike);

// Precomputed track geometry, built once by init_bike()/load_level().
// Points are absolute internal-X-sorted (strictly increasing px), so the
// renderer can binary-search the on-screen window instead of re-decoding the
// delta stream and projecting every point each frame.
typedef struct {
    const int* px;        // absolute X, internal units (strictly increasing)
    const int* py;        // absolute Y, internal units
    int count;
    int start_flag_idx;   // point index for the start flag (-1 if none)
    int finish_flag_idx;  // point index for the finish flag (-1 if none)
} TrackGeom;

const TrackGeom* physics_get_track_geom(void);

#endif // PHYSICS_H
