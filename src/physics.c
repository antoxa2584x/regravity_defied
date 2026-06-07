#include "physics.h"
#include "level.h"
#include "graphics.h"
#include <stdlib.h>

void init_bike(Bike* b, const uint8_t* track_data) {
    if (*track_data != 0x33) return;
    const uint8_t* p = track_data + 1;
    // start_x/start_y are already stored in internal F13 format: (value << 16) >> 3
    int32_t start_x = (int32_t)read_be32(p); p += 4;
    int32_t start_y = (int32_t)read_be32(p); p += 4;

    b->x = start_x;
    b->y = start_y;
    b->crash = 0;

    for (int i = 0; i < 6; i++) {
        b->nodes[i].x = b->x;
        b->nodes[i].y = b->y;
        b->nodes[i].old_x = b->x;
        b->nodes[i].old_y = b->y;
        b->nodes[i].radius = 0;
        b->nodes[i].grounded = 0;
    }

    // Nodes: 0=Frame, 1=FrontWheel, 2=RearWheel, 3=Fork, 4=Engine, 5=Rider
    b->nodes[1].x += convert_coord(WHEEL_DIST / 2);  // Front
    b->nodes[1].old_x = b->nodes[1].x;
    b->nodes[1].radius = convert_coord(4);
    
    b->nodes[2].x -= convert_coord(WHEEL_DIST / 2);  // Rear
    b->nodes[2].old_x = b->nodes[2].x;
    b->nodes[2].radius = convert_coord(4);
    
    b->nodes[3].x += convert_coord(WHEEL_DIST / 4);  // Fork
    b->nodes[3].y += convert_coord(9);
    b->nodes[3].old_x = b->nodes[3].x;
    b->nodes[3].old_y = b->nodes[3].y;
    
    b->nodes[0].y += convert_coord(10); // Frame center higher
    b->nodes[0].old_y = b->nodes[0].y;
    b->nodes[0].radius = convert_coord(2); // Smaller radius to avoid snagging

    b->nodes[4].y += convert_coord(7); // Engine/Bottom higher
    b->nodes[4].old_y = b->nodes[4].y;
    b->nodes[4].radius = convert_coord(3);

    b->nodes[5].y += convert_coord(22); // Rider even higher
    b->nodes[5].old_y = b->nodes[5].y;
    b->nodes[5].radius = convert_coord(2); // Smaller radius to avoid accidental floor touch
}

static void resolve_collision(PhysicsPoint* w, const uint8_t* track_data) {
    w->grounded = 0;
    if (!track_data || *track_data != 0x33) return;
    const uint8_t* p = track_data + 1 + 16;
    uint16_t points_count = read_be16(p); p += 2;
    int32_t cur_x = convert_coord(read_be32(p)); p += 4;
    int32_t cur_y = convert_coord(read_be32(p)); p += 4;

    int32_t wx = w->x;
    int32_t wy = w->y;
    int32_t wr = w->radius;
    int32_t margin = wr + convert_coord(2); // Small margin around node

    for (int i = 0; i < points_count - 1; i++) {
        int32_t next_x, next_y;
        int8_t dx = (int8_t)*p++;
        if (dx == -1) {
            cur_x = convert_coord(read_be32(p)); p += 4;
            cur_y = convert_coord(read_be32(p)); p += 4;
            continue;
        } else {
            int8_t dy = (int8_t)*p++;
            next_x = cur_x + convert_coord(dx);
            next_y = cur_y + convert_coord(dy);
        }

        int32_t x1 = cur_x, y1 = cur_y;
        int32_t x2 = next_x, y2 = next_y;

        // Skip segments that are definitely too far horizontally
        int32_t min_seg_x = x1 < x2 ? x1 : x2;
        int32_t max_seg_x = x1 > x2 ? x1 : x2;
        if (wx < min_seg_x - margin || wx > max_seg_x + margin) {
            cur_x = next_x;
            cur_y = next_y;
            continue;
        }

        int32_t dx_seg = x2 - x1;
        int32_t dy_seg = y2 - y1;
        
        // Vector from p1 to wheel
        int32_t wdx = wx - x1;
        int32_t wdy = wy - y1;
        
        // Project wheel onto segment: (W-P1) . (P2-P1) / |P2-P1|^2
        // Using 64-bit for intermediate to avoid overflow
        int64_t seg_len_sq = (int64_t)dx_seg * dx_seg + (int64_t)dy_seg * dy_seg;
        if (seg_len_sq > 0) {
            int64_t dot = (int64_t)wdx * dx_seg + (int64_t)wdy * dy_seg;
            int64_t t = (dot << 8) / seg_len_sq; // t in 8-bit fixed point (0..256)
            
            if (t >= 0 && t <= 256) {
                // Closest point is on segment
                int32_t closest_x = x1 + (int32_t)((int64_t)dx_seg * t >> 8);
                int32_t closest_y = y1 + (int32_t)((int64_t)dy_seg * t >> 8);
                
                int32_t dist_dx = wx - closest_x;
                int32_t dist_dy = wy - closest_y;
                int64_t dist_sq = (int64_t)dist_dx * dist_dx + (int64_t)dist_dy * dist_dy;
                
                if (dist_sq < (int64_t)wr * wr) {
                    // Collision!
                    // For bike physics, we usually just want to stay ABOVE the ground.
                    // If wy < closest_y + radius, push up.
                    // But we also need a normal for driving.
                    
                    // Simple approach: if we are "below" the line, push to surface.
                    // "Below" is determined by cross product or just Y comparison if we assume track is mostly left-to-right.
                    // The original code used wy <= gy + wr.
                    
                    // Let's find height at wx if it's within [x1, x2]
                    int32_t min_x = x1 < x2 ? x1 : x2;
                    int32_t max_x = x1 > x2 ? x1 : x2;
                    if (wx >= min_x && wx <= max_x && x1 != x2) {
                        int32_t gy = y1 + (int32_t)((int64_t)(y2 - y1) * (wx - x1) / (x2 - x1));
                        if (wy <= gy + wr) {
                            w->y = gy + wr;
                            // Sliding response for Verlet: 
                            // keep old_x to maintain horizontal momentum, 
                            // but adjust old_y to reduce bounce/sink.
                            // If we were falling fast, old_y would be much higher than y.
                            // Setting old_y = y kills vertical velocity.
                            w->old_y = w->y; 
                            w->grounded = 1;
                            w->nx = -(y2 - y1);
                            w->ny = (x2 - x1);
                        }
                    }
                }
            }
        }

        cur_x = next_x;
        cur_y = next_y;
    }
}

static void apply_constraint(PhysicsPoint* a, PhysicsPoint* b, int target_dist_fp) {
    int32_t dx = a->x - b->x;
    int32_t dy = a->y - b->y;
    
    // Better distance approximation: max(abs(dx), abs(dy)) + 0.5 * min(abs(dx), abs(dy))
    // Refined: (1007/1024) * max + (441/1024) * min
    int32_t adx = abs(dx);
    int32_t ady = abs(dy);
    int32_t max_d = (adx > ady) ? adx : ady;
    int32_t min_d = (adx > ady) ? ady : adx;
    int32_t dist = (max_d * 1007 + min_d * 441) >> 10;

    if (dist == 0) dist = 1;
    
    int32_t diff = (dist - target_dist_fp);
    if (abs(diff) < (1 << (FP_SHIFT - 8))) return; // Optimization: skip tiny adjustments

    // Use a small factor for spring-like behavior or higher for rigid. 
    // 0.5 (diff/2) is standard for Verlets.
    int32_t shift_x = (int64_t)dx * diff / dist / 2;
    int32_t shift_y = (int64_t)dy * diff / dist / 2;

    a->x -= shift_x;
    a->y -= shift_y;
    b->x += shift_x;
    b->y += shift_y;
}

void update_physics(Bike* b, const uint8_t* track_data, uint16_t keys) {
    b->crash = 0; // Reset crash state each frame
    // 1. Gravity & Integration (Verlet)
    for (int i = 0; i < 6; i++) {
        int32_t vx = b->nodes[i].x - b->nodes[i].old_x;
        int32_t vy = b->nodes[i].y - b->nodes[i].old_y;

        // Apply friction/damping
        int32_t f = b->nodes[i].grounded ? 254 : FRICTION;
        if (i == 1 || i == 2) f = b->nodes[i].grounded ? SURFACE_FRICTION : FRICTION;
        
        // Speed limit to prevent instability and potentially save some cycles in constraints
        if (vx > convert_coord(10)) vx = convert_coord(10);
        if (vx < -convert_coord(10)) vx = -convert_coord(10);
        if (vy > convert_coord(10)) vy = convert_coord(10);
        if (vy < -convert_coord(10)) vy = -convert_coord(10);

        vx = (int64_t)vx * f / 256;
        vy = (int64_t)vy * f / 256;

        b->nodes[i].old_x = b->nodes[i].x;
        b->nodes[i].old_y = b->nodes[i].y;

        b->nodes[i].x += vx;
        b->nodes[i].y += vy - convert_coord(1) / 8; // Increased Gravity
    }

    // 2. Drive Logic (Rear wheel is node 2)
    if (keys & KEY_UP) {
        if (b->nodes[2].grounded) {
            int32_t tx = b->nodes[2].ny;
            int32_t ty = -b->nodes[2].nx;
            int32_t mag = abs(tx) + abs(ty);
            if (mag > 0) {
                b->nodes[2].x += (int64_t)tx * convert_coord(ACCEL) / mag / 32; // More power
                b->nodes[2].y += (int64_t)ty * convert_coord(ACCEL) / mag / 32;
            }
        }
    }

    // 3. Lean Logic
    if (keys & KEY_LEFT) {
        // Apply torque: push rider left, push frame right
        b->nodes[5].x -= convert_coord(1) / 16;
        b->nodes[0].x += convert_coord(1) / 32;
    }
    if (keys & KEY_RIGHT) {
        // Apply torque: push rider right, push frame left
        b->nodes[5].x += convert_coord(1) / 16;
        b->nodes[0].x -= convert_coord(1) / 32;
    }

    // 4. Collision
    for (int i = 0; i < 6; i++) {
        if (b->nodes[i].radius > 0) {
            resolve_collision(&b->nodes[i], track_data);
        }
    }

    // 5. Constraints (Verlet-friendly)
    int32_t target_dist = convert_coord(WHEEL_DIST);
    for (int iter = 0; iter < 5; iter++) { // Slightly increased iterations for better rigidity
        // Frame to Wheels
        apply_constraint(&b->nodes[0], &b->nodes[1], target_dist / 2);
        apply_constraint(&b->nodes[0], &b->nodes[2], target_dist / 2);
        // Fork
        apply_constraint(&b->nodes[0], &b->nodes[3], target_dist / 4);
        apply_constraint(&b->nodes[3], &b->nodes[1], target_dist / 4);
        // Engine
        apply_constraint(&b->nodes[4], &b->nodes[0], convert_coord(5));
        apply_constraint(&b->nodes[4], &b->nodes[1], target_dist * 3 / 4);
        apply_constraint(&b->nodes[4], &b->nodes[2], target_dist * 3 / 4);
        // Rider - further from frame center and stabilized by connecting to fork
        apply_constraint(&b->nodes[5], &b->nodes[0], target_dist * 3 / 4);
        apply_constraint(&b->nodes[5], &b->nodes[3], target_dist * 3 / 4); // Keep rider upright
        // Cross-constraint for wheelbase stability
        apply_constraint(&b->nodes[1], &b->nodes[2], target_dist);
    }

    // 6. Crash Detection
    if (b->nodes[5].grounded) {
        b->crash = 1;
    }

    int32_t dx_wheels = b->nodes[1].x - b->nodes[2].x;
    int32_t dy_wheels = b->nodes[1].y - b->nodes[2].y;
    int32_t w_dist = abs(dx_wheels) + abs(dy_wheels);
    if (w_dist < target_dist / 2 || w_dist > target_dist * 2) {
        // b->crash = 1;
    }

    // 6. Center for view
    b->x = b->nodes[0].x;
    b->y = b->nodes[0].y;
}

void draw_bike(Bike* b, int ox, int oy) {
    int px[6], py[6];
    for (int i = 0; i < 6; i++) {
        px[i] = get_pixel_coord(b->nodes[i].x) + ox;
        py[i] = oy - get_pixel_coord(b->nodes[i].y);
    }
    
    // Draw wheels
    draw_rect(px[1] - 2, py[1] - 2, 4, 4, COLOR(0, 0, 0));
    draw_rect(px[2] - 2, py[2] - 2, 4, 4, COLOR(0, 0, 0));

    // Draw frame
    draw_line(px[0], py[0], px[1], py[1], COLOR(31, 0, 0));
    draw_line(px[0], py[0], px[2], py[2], COLOR(31, 0, 0));
    draw_line(px[0], py[0], px[4], py[4], COLOR(15, 15, 15)); // Engine
    draw_line(px[0], py[0], px[5], py[5], COLOR(0, 0, 31)); // Rider
}
