#ifndef PHYSICS_H
#define PHYSICS_H

#include "gba.h"

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

#endif // PHYSICS_H
