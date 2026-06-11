// Faithful 1:1 port of the reference Gravity Defied physics engine
// (ref/src/GamePhysics.cpp + the track-collision parts of LevelLoader/GameLevel).
//
// Coordinate conventions copied from the reference:
//   * Track points (lvl_px/lvl_py) are in 1x internal units: (value << 16) >> 3.
//   * Moto node positions (Node.mc[].x/.y) are in 2x internal units.
//   * Collision halves node coords (>>1) to compare against track points.
//
// Each node (class_10) keeps 6 sub-states (MotoPart mc[6]):
//   index01 / index10 : ping-pong current / next state
//   2,3,4             : integration temporaries
//   5                 : (render snapshot in ref; we render from index01)

#include "physics.h"
#include "level.h"
#include "graphics.h"
#include <stdlib.h>

#define MAXP 160

#define mulF(a, b) ((int)(((int64_t)(a) * (int64_t)(b)) >> 16))
#define divF(a, b) ((int)((((int64_t)(a)) << 32) / (int64_t)(b) >> 16))

// ---- moto state -----------------------------------------------------------

typedef struct {
    int x, y;          // xF16, yF16 (2x internal)
    int angle;         // angleF16 (wheel rotation)
    int f382, f383;    // velocity x/y
    int f384;          // wheel angular speed
    int f385, f386, f387; // force accumulators
} MotoPart;

typedef struct {
    int f257;          // collision radius (const175_1_half[class])
    int f258;          // radius class index
    int f259;          // inverse-mass scaled
    int f260;          // engine torque coupling
    MotoPart mc[6];
} Node;

static const int const175_1_half[3] = { 114688, 65536, 32768 };

static Node     nodes[6];     // field_29
static MotoPart springs[10];  // field_30 (x=stiffness, y=rest, angle=damping)

static int index01 = 0, index10 = 1;

// league / mode params
static int field_7, field_8, field_9, field_10, field_11, field_14, field_16, field_19;
static int motoParam1, motoParam2, motoParam3, motoParam4, motoParam5,
           motoParam6, motoParam7, motoParam8, motoParam9, motoParam10;
static int cur_league = 1;

// running physics state
static int field_31;          // engine drive accumulator
static int field_37 = 32768;  // lean blend
static int field_39;          // wheel-relative speed signed
static int field_28 = -1;     // last colliding node
static int field_33, field_34; // collision normal
static int field_35, field_36, field_68, field_69;

// inputs
static int isInputAcceleration, isInputBreak, isInputBack, isInputForward;

// ---- level (track) collision model ----------------------------------------

static int lvl_px[MAXP], lvl_py[MAXP];     // pointPositions (1x internal)
static int lvl_count;
static int lvl_nx[MAXP], lvl_ny[MAXP];     // field_121 segment normals (F16)
static int lvl_startPosX, lvl_startPosY, lvl_finishPosX, lvl_finishPosY;
static int startFlagPoint, finishFlagPoint;

static int field_123[3], field_124[3];     // outer / inner radius^2 (>>16)
static int field_133, field_134, field_135, field_136; // nearest-segment window
static int field_137, field_138;           // collision normal out
static int isEnabledPerspective = 1;

static int smth_max_abs(int x, int y) {
    int ax = x < 0 ? -x : x;
    int ay = y < 0 ? -y : y;
    int mx, mn;
    if (ay >= ax) { mx = ay; mn = ax; } else { mx = ax; mn = ay; }
    return mulF(64448, mx) + mulF(28224, mn);
}

// Decode track points (1x internal) and precompute normals + flag points.
static void load_level(const uint8_t* data) {
    const uint8_t* p = data;
    lvl_count = 0;
    startFlagPoint = 0;
    finishFlagPoint = 0;
    if (*p != 0x33) return;
    p++;
    lvl_startPosX  = (int)read_be32(p); p += 4;
    lvl_startPosY  = (int)read_be32(p); p += 4;
    lvl_finishPosX = (int)read_be32(p); p += 4;
    lvl_finishPosY = (int)read_be32(p); p += 4;
    uint16_t pc = read_be16(p); p += 2;

    int cx = convert_coord((int)read_be32(p)); p += 4;
    int cy = convert_coord((int)read_be32(p)); p += 4;
    lvl_px[lvl_count] = cx; lvl_py[lvl_count] = cy; lvl_count++;

    for (int i = 1; i < pc && lvl_count < MAXP; i++) {
        int8_t dx = (int8_t)*p++;
        if (dx == -1) {
            cx = convert_coord((int)read_be32(p)); p += 4;
            cy = convert_coord((int)read_be32(p)); p += 4;
        } else {
            int8_t dy = (int8_t)*p++;
            cx += convert_coord(dx);
            cy += convert_coord(dy);
        }
        // addPoint: only keep strictly increasing x (matches ref addPoint)
        if (lvl_count == 0 || lvl_px[lvl_count - 1] < cx) {
            lvl_px[lvl_count] = cx; lvl_py[lvl_count] = cy; lvl_count++;
        }
    }

    // method_96: segment normals + flag points
    for (int i = 0; i < lvl_count; i++) {
        int j = (i + 1) % lvl_count;
        int dx = lvl_px[j] - lvl_px[i];
        int dy = lvl_py[j] - lvl_py[i];
        int nrm = -dy;
        int mag = smth_max_abs(nrm, dx);
        if (mag == 0) mag = 1;
        lvl_nx[i] = divF(nrm, mag);
        lvl_ny[i] = divF(dx, mag);
        if (startFlagPoint == 0 && lvl_px[i] > lvl_startPosX) startFlagPoint = i + 1;
        if (finishFlagPoint == 0 && lvl_px[i] > lvl_finishPosX) finishFlagPoint = i;
    }
    if (startFlagPoint >= lvl_count) startFlagPoint = lvl_count - 1;
    if (finishFlagPoint >= lvl_count) finishFlagPoint = lvl_count - 1;
}

// LevelLoader::method_100 — slide the nearest-segment window to cover [var1,var2].
static void lvl_window(int var1, int var2) {
    var2 >>= 1; var1 >>= 1;
    if (field_134 > lvl_count - 1) field_134 = lvl_count - 1;
    if (field_133 < 0) field_133 = 0;
    if (var2 > field_136) {
        while (field_134 < lvl_count - 1 && var2 > lvl_px[++field_134]) {}
    } else if (var1 < field_135) {
        while (field_133 > 0 && var1 < lvl_px[--field_133]) {}
    } else {
        while (field_133 < lvl_count - 1 && var1 > lvl_px[field_133 + 1]) { ++field_133; }
        while (field_134 > 0 && var2 < lvl_px[--field_134]) {}
        field_134 = (field_134 + 1 < lvl_count - 1) ? field_134 + 1 : lvl_count - 1;
    }
    field_135 = lvl_px[field_133];
    field_136 = lvl_px[field_134];
}

// LevelLoader::method_101 — collide one moto point against the window.
// Returns 2 free, 1 touching (apply response), 0 deep penetration.
static int lvl_collide(MotoPart* pt, int rclass) {
    int var16 = 0;
    int var17 = 2;
    int v18 = pt->x >> 1;
    int v19 = pt->y >> 1;
    if (isEnabledPerspective) v19 -= 65536;
    int v20 = 0, v21 = 0;

    for (int s = field_133; s < field_134; s++) {
        int x4 = lvl_px[s];
        int y5 = lvl_py[s];
        int x6 = lvl_px[s + 1];
        int y7 = lvl_py[s + 1];

        if (v18 - field_123[rclass] <= x6 && v18 + field_123[rclass] >= x4) {
            int a8 = x4 - x6;
            int a9 = y5 - y7;
            int a10 = mulF(a8, a8) + mulF(a9, a9);
            int a11 = mulF(v18 - x4, -a8) + mulF(v19 - y5, -a9);
            int t;
            if ((a10 < 0 ? -a10 : a10) >= 3) t = divF(a11, a10);
            else t = (a11 > 0 ? 1 : -1) * (a10 > 0 ? 1 : -1) * 0x7FFFFFFF;
            if (t < 0) t = 0;
            if (t > 65536) t = 65536;

            int cxp = x4 + mulF(t, -a8);
            int cyp = y5 + mulF(t, -a9);
            int dx = v18 - cxp;
            int dy = v19 - cyp;
            int64_t d2 = (int64_t)mulF(dx, dx) + (int64_t)mulF(dy, dy);

            int kind;
            if (d2 < (int64_t)field_123[rclass]) {
                kind = (d2 >= (int64_t)field_124[rclass]) ? 1 : 0;
            } else {
                kind = 2;
            }

            if (kind == 0 && (mulF(lvl_nx[s], pt->f382) + mulF(lvl_ny[s], pt->f383)) < 0) {
                field_137 = lvl_nx[s];
                field_138 = lvl_ny[s];
                return 0;
            }
            if (kind == 1 && (mulF(lvl_nx[s], pt->f382) + mulF(lvl_ny[s], pt->f383)) < 0) {
                var16++;
                var17 = 1;
                if (var16 == 1) { v20 = lvl_nx[s]; v21 = lvl_ny[s]; }
                else { v20 += lvl_nx[s]; v21 += lvl_ny[s]; }
            }
        }
    }

    if (var17 == 1) {
        if (mulF(v20, pt->f382) + mulF(v21, pt->f383) >= 0) return 2;
        field_137 = v20;
        field_138 = v21;
    }
    return var17;
}

// ---- league / setup --------------------------------------------------------

void physics_set_league(int league) {
    if (league < 0) league = 0;
    if (league > 2) league = 2;
    cur_league = league;
}

static void set_league(int league) {
    field_9 = 45875; field_10 = 13107; field_11 = 39321;
    field_14 = 1310720; field_16 = 262144; field_19 = 6553;
    switch (league) {
    case 0:
        motoParam1 = 19660; motoParam2 = 19660; motoParam3 = 1114112;
        motoParam4 = 52428800; motoParam5 = 3276800; motoParam6 = 327;
        motoParam7 = 0; motoParam8 = 32768; motoParam9 = 327680;
        motoParam10 = 19660800; break;
    case 1:
    default:
        motoParam1 = 32768; motoParam2 = 32768; motoParam3 = 1114112;
        motoParam4 = 65536000; motoParam5 = 3276800; motoParam6 = 6553;
        motoParam7 = 26214; motoParam8 = 26214; motoParam9 = 327680;
        motoParam10 = 19660800; break;
    case 2:
        motoParam1 = 32768; motoParam2 = 32768; motoParam3 = 1310720;
        motoParam4 = 75366400; motoParam5 = 3473408; motoParam6 = 6553;
        motoParam7 = 26214; motoParam8 = 39321; motoParam9 = 327680;
        motoParam10 = 21626880; break;
    }
}

static void zero_part(MotoPart* m) {
    m->x = m->y = m->angle = 0;
    m->f382 = m->f383 = m->f384 = 0;
    m->f385 = m->f386 = m->f387 = 0;
}

// GamePhysics::method_27 — build nodes & springs at the start position.
static void build_moto(int sx, int sy) {
    for (int i = 0; i < 6; i++) {
        int cls = 0, mass = 0, ox = 0, oy = 0, f260 = 0;
        switch (i) {
        case 0: cls = 1; mass = 360448; ox = 0;       oy = 0;      f260 = 0;     break;
        case 1: cls = 0; mass = 98304;  ox = 229376;  oy = 0;      f260 = 0;     break;
        case 2: cls = 0; mass = 360448; ox = -229376; oy = 0;      f260 = 21626; break;
        case 3: cls = 1; mass = 229376; ox = 131072;  oy = 196608; f260 = 0;     break;
        case 4: cls = 1; mass = 229376; ox = -131072; oy = 196608; f260 = 0;     break;
        case 5: cls = 2; mass = 294912; ox = 0;       oy = 327680; f260 = 0;     break;
        }
        for (int s = 0; s < 6; s++) zero_part(&nodes[i].mc[s]);
        nodes[i].f257 = const175_1_half[cls];
        nodes[i].f258 = cls;
        nodes[i].f259 = mulF((int)(281474976710656LL / (int64_t)mass >> 16), field_14);
        nodes[i].mc[index01].x = sx + ox;
        nodes[i].mc[index01].y = sy + oy;
        nodes[i].mc[5].x = sx + ox;
        nodes[i].mc[5].y = sy + oy;
        nodes[i].f260 = f260;
    }

    for (int i = 0; i < 10; i++) {
        zero_part(&springs[i]);
        springs[i].x = motoParam10;
        springs[i].angle = field_16;
    }
    springs[0].y = 229376; springs[1].y = 229376;
    springs[2].y = 236293; springs[3].y = 236293;
    springs[4].y = 262144;
    springs[5].y = 219814; springs[6].y = 219814;
    springs[7].y = 185363; springs[8].y = 185363;
    springs[9].y = 327680;
    springs[5].angle = mulF(field_16, 45875);
    springs[6].x = mulF(6553, motoParam10);
    springs[5].x = mulF(6553, motoParam10);
    springs[9].x = mulF(72089, motoParam10);
    springs[8].x = mulF(72089, motoParam10);
    springs[7].x = mulF(72089, motoParam10);
}

// ---- solver ----------------------------------------------------------------

// GamePhysics::method_42 — spring force between two nodes' parts.
static void spring_force(Node* A, MotoPart* sp, Node* B, int slot, int scale) {
    MotoPart* a = &A->mc[slot];
    MotoPart* b = &B->mc[slot];
    int dx = a->x - b->x;
    int dy = a->y - b->y;
    int dist = smth_max_abs(dx, dy);
    if ((dist < 0 ? -dist : dist) >= 3) {
        int ux = divF(dx, dist);
        int uy = divF(dy, dist);
        int stretch = dist - sp->y;
        int base = mulF(stretch, sp->x);
        int fx = mulF(ux, base);
        int fy = mulF(uy, base);
        int rvx = a->f382 - b->f382;
        int rvy = a->f383 - b->f383;
        int damp = mulF(mulF(ux, rvx) + mulF(uy, rvy), sp->angle);
        fx += mulF(ux, damp);
        fy += mulF(uy, damp);
        fx = mulF(fx, scale);
        fy = mulF(fy, scale);
        a->f385 -= fx; a->f386 -= fy;
        b->f385 += fx; b->f386 += fy;
    }
}

// GamePhysics::method_40 — accumulate forces (gravity + springs + engine).
static void accumulate_forces(int slot) {
    for (int i = 0; i < 6; i++) {
        MotoPart* m = &nodes[i].mc[slot];
        m->f385 = 0;
        m->f386 = 0;
        m->f387 = 0;
        m->f386 -= divF(field_8, nodes[i].f259);
    }

    if (!field_35) {
        spring_force(&nodes[0], &springs[1], &nodes[2], slot, 65536);
        spring_force(&nodes[0], &springs[0], &nodes[1], slot, 65536);
        spring_force(&nodes[2], &springs[6], &nodes[4], slot, 131072);
        spring_force(&nodes[1], &springs[5], &nodes[3], slot, 131072);
    }
    spring_force(&nodes[0], &springs[2], &nodes[3], slot, 65536);
    spring_force(&nodes[0], &springs[3], &nodes[4], slot, 65536);
    spring_force(&nodes[3], &springs[4], &nodes[4], slot, 65536);
    spring_force(&nodes[5], &springs[8], &nodes[3], slot, 65536);
    spring_force(&nodes[5], &springs[7], &nodes[4], slot, 65536);
    spring_force(&nodes[5], &springs[9], &nodes[0], slot, 65536);

    MotoPart* rear = &nodes[2].mc[slot];
    field_31 = mulF(field_31, 65536 - field_19);
    rear->f387 = field_31;
    if (rear->f384 > motoParam3) rear->f384 = motoParam3;
    if (rear->f384 < -motoParam3) rear->f384 = -motoParam3;

    // remove net momentum / clamp angular spread
    int sx = 0, sy = 0;
    for (int i = 0; i < 6; i++) { sx += nodes[i].mc[slot].f382; sy += nodes[i].mc[slot].f383; }
    sx = divF(sx, 393216);
    sy = divF(sy, 393216);
    int mag = 0;
    for (int i = 0; i < 6; i++) {
        int vx = nodes[i].mc[slot].f382 - sx;
        int vy = nodes[i].mc[slot].f383 - sy;
        if ((mag = smth_max_abs(vx, vy)) > 1966080) {
            nodes[i].mc[slot].f382 -= divF(vx, mag);
            nodes[i].mc[slot].f383 -= divF(vy, mag);
        }
    }
    int s1 = (nodes[2].mc[slot].y - nodes[0].mc[slot].y) >= 0 ? 1 : -1;
    int s2 = (nodes[2].mc[slot].f382 - nodes[0].mc[slot].f382) >= 0 ? 1 : -1;
    field_39 = (s1 * s2 > 0) ? mag : -mag;
}

// GamePhysics::method_43 — derivative of state into slot_out.
static void derive(int slot_in, int slot_out, int dt) {
    for (int i = 0; i < 6; i++) {
        MotoPart* s = &nodes[i].mc[slot_in];
        MotoPart* d = &nodes[i].mc[slot_out];
        d->x = mulF(s->f382, dt);
        d->y = mulF(s->f383, dt);
        int v = mulF(dt, nodes[i].f259);
        d->f382 = mulF(s->f385, v);
        d->f383 = mulF(s->f386, v);
    }
}

// GamePhysics::method_44 — slot_out = slot_a + slot_b/2.
static void blend(int slot_out, int slot_a, int slot_b) {
    for (int i = 0; i < 6; i++) {
        MotoPart* d = &nodes[i].mc[slot_out];
        MotoPart* a = &nodes[i].mc[slot_a];
        MotoPart* b = &nodes[i].mc[slot_b];
        d->x = a->x + (b->x >> 1);
        d->y = a->y + (b->y >> 1);
        d->f382 = a->f382 + (b->f382 >> 1);
        d->f383 = a->f383 + (b->f383 >> 1);
    }
}

// GamePhysics::method_45 — one integration step of size dt.
static void integrate(int dt) {
    accumulate_forces(index01);
    derive(index01, 2, dt);
    blend(4, index01, 2);
    accumulate_forces(4);
    derive(4, 3, dt >> 1);
    blend(4, index01, 3);
    blend(index10, index01, 2);
    blend(index10, index10, 3);

    for (int i = 1; i <= 2; i++) {
        MotoPart* cur = &nodes[i].mc[index01];
        MotoPart* nxt = &nodes[i].mc[index10];
        nxt->angle = cur->angle + mulF(dt, cur->f384);
        nxt->f384  = cur->f384 + mulF(dt, mulF(nodes[i].f260, cur->f387));
    }
}

// GamePhysics::isTrackStarted / method_38
static int track_started(void) __attribute__((unused));
static int track_started(void) {
    return nodes[1].mc[index01].x < (lvl_px[startFlagPoint] << 1);
}
static int reached_finish(void) {
    int fx = lvl_px[finishFlagPoint] << 1;
    return nodes[1].mc[index10].x > fx || nodes[2].mc[index10].x > fx;
}

// GamePhysics::method_46 — test all collidable nodes against the track.
static int collide_all(int slot) {
    int ret = 2;
    int a = nodes[1].mc[slot].x, b = nodes[2].mc[slot].x, c = nodes[5].mc[slot].x;
    int mx = a > b ? a : b; if (c > mx) mx = c;
    int mn = a < b ? a : b; if (c < mn) mn = c;
    lvl_window(mn - const175_1_half[0], mx + const175_1_half[0]);

    int dx = nodes[1].mc[slot].x - nodes[2].mc[slot].x;
    int dy = nodes[1].mc[slot].y - nodes[2].mc[slot].y;
    int mag = smth_max_abs(dx, dy);
    if (mag == 0) mag = 1;
    int ux = divF(dx, mag);
    int uy = -divF(dy, mag);
    int perpx = uy;   // var9
    int perpy = ux;   // var10

    for (int i = 0; i < 6; i++) {
        if (i == 3 || i == 4) continue;
        MotoPart* pt = &nodes[i].mc[slot];
        if (i == 0) { pt->x += perpx; pt->y += perpy; }
        int r = lvl_collide(pt, nodes[i].f258);
        if (i == 0) { pt->x -= perpx; pt->y -= perpy; }
        field_33 = field_137;
        field_34 = field_138;
        if (i == 5 && r != 2) field_36 = 1;
        if (i == 1 && r != 2) field_69 = 1;
        if (r == 1) { field_28 = i; ret = 1; }
        else if (r == 0) { field_28 = i; ret = 0; break; }
    }
    return ret;
}

// GamePhysics::method_47 — collision response on node field_28.
static void collide_response(int slot) {
    Node* nd = &nodes[field_28];
    MotoPart* m = &nd->mc[slot];
    m->x += mulF(field_33, 3276);
    m->y += mulF(field_34, 3276);

    int c9, c10, c11, c1, c2;
    if (isInputBreak && (field_28 == 2 || field_28 == 1) && m->f384 < 6553) {
        c9 = field_9 - motoParam7;
        c10 = 13107;
        c11 = 39321;
        c1 = 26214 - motoParam7;
        c2 = 26214 - motoParam7;
    } else {
        c9 = field_9; c10 = field_10; c11 = field_11;
        c1 = motoParam1; c2 = motoParam2;
    }

    int mag = smth_max_abs(field_33, field_34);
    if (mag == 0) mag = 1;
    field_33 = divF(field_33, mag);
    field_34 = divF(field_34, mag);
    int vx = m->f382;
    int vy = m->f383;
    int along  = -(mulF(vx, field_33) + mulF(vy, field_34));
    int across = -(mulF(vx, -field_34) + mulF(vy, field_33));
    int nf384  = mulF(c9, m->f384) - mulF(c10, divF(across, nd->f257));
    int fr     = mulF(c1, across) - mulF(c11, mulF(m->f384, nd->f257));
    int fn     = -mulF(c2, along);
    int r17 = mulF(-fr, -field_34);
    int r18 = mulF(-fr, field_33);
    int r19 = mulF(-fn, field_33);
    int r20 = mulF(-fn, field_34);
    m->f384 = nf384;
    m->f382 = r17 + r19;
    m->f383 = r18 + r20;
}

// GamePhysics::method_39 — adaptive sub-stepping driver.
static int step_solver(int total) {
    int started = field_68;
    int var3 = 0;
    int var4 = total;

label77:
    do {
        int r;
        while (var3 < total) {
            integrate(var4 - var3);
            if (!started && reached_finish()) r = 3;
            else r = collide_all(index10);

            if (!started && field_68) {
                return (r != 3) ? 2 : 1;
            }
            if (r == 0) { var4 = (var3 + var4) >> 1; goto label77; }
            if (r == 3) {
                field_68 = 1;
                var4 = (var3 + var4) >> 1;
            } else {
                if (r == 1) {
                    int rr;
                    do {
                        collide_response(index10);
                        if ((rr = collide_all(index10)) == 0) return 5;
                    } while (rr != 2);
                }
                var3 = var4;
                var4 = total;
                index01 = index01 == 1 ? 0 : 1;
                index10 = index10 == 1 ? 0 : 1;
            }
        }

        {
            int dx = nodes[1].mc[index01].x - nodes[2].mc[index01].x;
            int dy = nodes[1].mc[index01].y - nodes[2].mc[index01].y;
            int d2 = mulF(dx, dx) + mulF(dy, dy);
            if (d2 < 983040) field_35 = 1;
            if (d2 > 4587520) field_35 = 1;
        }
        return 0;
    } while ((((var4 = (var3 + var4) >> 1) - var3) < 0 ? -(var4 - var3) : (var4 - var3)) >= 65);

    return 5;
}

// GamePhysics::method_35 — translate inputs into engine drive / lean torque.
static void apply_input(void) {
    if (field_35) return;
    int dx = nodes[1].mc[index01].x - nodes[2].mc[index01].x;
    int dy = nodes[1].mc[index01].y - nodes[2].mc[index01].y;
    int mag = smth_max_abs(dx, dy);
    if (mag == 0) mag = 1;
    int ux = divF(dx, mag);
    int uy = divF(dy, mag);

    if (isInputAcceleration && field_31 >= -motoParam4) field_31 -= motoParam5;

    if (isInputBreak) {
        field_31 = 0;
        nodes[1].mc[index01].f384 = mulF(nodes[1].mc[index01].f384, 65536 - motoParam6);
        nodes[2].mc[index01].f384 = mulF(nodes[2].mc[index01].f384, 65536 - motoParam6);
        if (nodes[1].mc[index01].f384 < 6553) nodes[1].mc[index01].f384 = 0;
        if (nodes[2].mc[index01].f384 < 6553) nodes[2].mc[index01].f384 = 0;
    }

    nodes[0].f259 = mulF(11915, field_14);
    nodes[4].f259 = mulF(18724, field_14);
    nodes[3].f259 = mulF(18724, field_14);
    nodes[1].f259 = mulF(43690, field_14);
    nodes[2].f259 = mulF(11915, field_14);
    nodes[5].f259 = mulF(14563, field_14);
    if (isInputBack) {
        nodes[0].f259 = mulF(18724, field_14);
        nodes[4].f259 = mulF(14563, field_14);
        nodes[3].f259 = mulF(18724, field_14);
        nodes[1].f259 = mulF(43690, field_14);
        nodes[2].f259 = mulF(10082, field_14);
    } else if (isInputForward) {
        nodes[0].f259 = mulF(18724, field_14);
        nodes[4].f259 = mulF(18724, field_14);
        nodes[3].f259 = mulF(14563, field_14);
        nodes[1].f259 = mulF(26214, field_14);
        nodes[2].f259 = mulF(11915, field_14);
    }

    if (isInputBack || isInputForward) {
        int v4 = -uy;
        if (isInputBack && field_39 > -motoParam9) {
            int v6 = 65536;
            if (field_39 < 0) v6 = divF(motoParam9 - (field_39 < 0 ? -field_39 : field_39), motoParam9);
            int v7 = mulF(motoParam8, v6);
            int v8 = mulF(v4, v7);
            int v9 = mulF(ux, v7);
            int v10 = mulF(ux, v7);
            int v11 = mulF(uy, v7);
            field_37 = (field_37 > 32768) ? ((field_37 - 1638 < 0) ? 0 : field_37 - 1638)
                                          : ((field_37 - 3276 < 0) ? 0 : field_37 - 3276);
            nodes[4].mc[index01].f382 -= v8; nodes[4].mc[index01].f383 -= v9;
            nodes[3].mc[index01].f382 += v8; nodes[3].mc[index01].f383 += v9;
            nodes[5].mc[index01].f382 -= v10; nodes[5].mc[index01].f383 -= v11;
        }
        if (isInputForward && field_39 < motoParam9) {
            int v6 = 65536;
            if (field_39 > 0) v6 = divF(motoParam9 - field_39, motoParam9);
            int v7 = mulF(motoParam8, v6);
            int v8 = mulF(v4, v7);
            int v9 = mulF(ux, v7);
            int v10 = mulF(ux, v7);
            int v11 = mulF(uy, v7);
            field_37 = (field_37 > 32768) ? ((field_37 + 1638 < 65536) ? field_37 + 1638 : 65536)
                                          : ((field_37 + 3276 < 65536) ? field_37 + 3276 : 65536);
            nodes[4].mc[index01].f382 += v8; nodes[4].mc[index01].f383 += v9;
            nodes[3].mc[index01].f382 -= v8; nodes[3].mc[index01].f383 -= v9;
            nodes[5].mc[index01].f382 += v10; nodes[5].mc[index01].f383 += v11;
        }
        return;
    }

    if (field_37 < 26214) { field_37 += 3276; return; }
    if (field_37 > 39321) { field_37 -= 3276; return; }
    field_37 = 32768;
}

// ---- public interface ------------------------------------------------------

void init_bike(Bike* b, const uint8_t* track_data) {
    load_level(track_data);
    set_league(cur_league);
    field_7 = 1310;
    field_8 = 1638400;

    // LevelLoader ctor: radius^2 thresholds per class.
    for (int i = 0; i < 3; i++) {
        int outer = (const175_1_half[i] + 19660) >> 1;
        int inner = (const175_1_half[i] - 19660) >> 1;
        field_123[i] = (int)(((int64_t)outer * outer) >> 16);
        field_124[i] = (int)(((int64_t)inner * inner) >> 16);
    }

    index01 = 0; index10 = 1;
    field_31 = 0; field_37 = 32768; field_39 = 0;
    field_28 = -1; field_33 = field_34 = 0;
    field_35 = field_36 = field_68 = field_69 = 0;
    field_133 = field_134 = field_135 = field_136 = 0;

    build_moto(lvl_startPosX << 1, lvl_startPosY << 1);

    b->crash = 0;
    b->x = nodes[0].mc[index01].x >> 1;
    b->y = nodes[0].mc[index01].y >> 1;
    for (int i = 0; i < 6; i++) {
        b->nodes[i].x = nodes[i].mc[index01].x >> 1;
        b->nodes[i].y = nodes[i].mc[index01].y >> 1;
    }
}

void update_physics(Bike* b, const uint8_t* track_data, uint16_t keys) {
    (void)track_data;
    isInputAcceleration = (keys & KEY_UP) ? 1 : 0;
    isInputBreak        = (keys & KEY_DOWN) ? 1 : 0;
    isInputBack         = (keys & KEY_LEFT) ? 1 : 0;
    isInputForward      = (keys & KEY_RIGHT) ? 1 : 0;

    field_36 = 0;
    field_69 = 0;
    apply_input();
    step_solver(field_7);

    b->crash = field_36;
    b->x = nodes[0].mc[index01].x >> 1;
    b->y = nodes[0].mc[index01].y >> 1;
    for (int i = 0; i < 6; i++) {
        b->nodes[i].x = nodes[i].mc[index01].x >> 1;
        b->nodes[i].y = nodes[i].mc[index01].y >> 1;
    }
}

// 32-entry sin table, F8 (sin * 256), index = angle * 32 / 2pi.
static const short SIN32[32] = {
    0,  50,  98, 142, 181, 213, 237, 251, 256, 251, 237, 213, 181, 142, 98, 50,
    0, -50, -98,-142,-181,-213,-237,-251,-256,-251,-237,-213,-181,-142,-98,-50
};
// angle is F16 radians; reduce to [0,32).
static int ang_idx(int angleF16) {
    // idx = angle / (2pi) * 32 = angle * 32 / 411775  (2pi*65536=411775)
    int i = (int)(((int64_t)angleF16 * 32) / 411775);
    i &= 31;
    if (i < 0) i += 32;
    return i;
}
static int isin(int idx) { return SIN32[idx & 31]; }
static int icos(int idx) { return SIN32[(idx + 8) & 31]; }

// Draw a spoked wheel: tire ring + spokes rotated by the wheel spin angle.
static void draw_wheel(int cx, int cy, int angleF16) {
    fill_circle(cx, cy, 4, COLOR(2, 2, 2));      // tire
    fill_circle(cx, cy, 2, COLOR(12, 12, 12));   // hub area
    int base = ang_idx(angleF16);
    for (int s = 0; s < 4; s++) {
        int a = (base + s * 8) & 31;
        int ex = cx + isin(a) * 4 / 256;
        int ey = cy - icos(a) * 4 / 256;
        draw_line(cx, cy, ex, ey, COLOR(20, 20, 20)); // spokes
    }
    put_pixel(cx, cy, COLOR(28, 28, 28));            // hub
}

void draw_bike(Bike* b, int ox, int oy) {
    int px[6], py[6];
    for (int i = 0; i < 6; i++) {
        px[i] = get_pixel_coord(b->nodes[i].x) + ox;
        py[i] = oy - get_pixel_coord(b->nodes[i].y);
    }

    // Frame / fork (dark), styled after the ref engine/fender parts.
    draw_line(px[3], py[3], px[1], py[1], COLOR(18, 18, 18)); // front fork
    draw_line(px[4], py[4], px[2], py[2], COLOR(18, 18, 18)); // rear arm
    draw_line(px[0], py[0], px[3], py[3], COLOR(6, 6, 8));
    draw_line(px[0], py[0], px[4], py[4], COLOR(6, 6, 8));
    draw_line(px[3], py[3], px[4], py[4], COLOR(6, 6, 8));    // engine block line

    // Spoked wheels using real wheel-spin angle from the solver.
    draw_wheel(px[1], py[1], nodes[1].mc[index01].angle);
    draw_wheel(px[2], py[2], nodes[2].mc[index01].angle);

    // Rider: blue body (legs->frame->torso) + helmet, styled after blue* parts.
    int hx = px[5], hy = py[5];
    int mx = (px[0] + px[5]) / 2, my = (py[0] + py[5]) / 2;
    draw_line(px[0], py[0], mx, my, COLOR(2, 4, 22));  // legs
    draw_line(mx, my, hx, hy, COLOR(4, 8, 28));        // torso (blue body)
    draw_line(mx, my, px[3], py[3], COLOR(4, 8, 28));  // arm to bars
    fill_circle(hx, hy, 3, COLOR(28, 22, 14));         // helmet
    fill_circle(hx, hy, 2, COLOR(20, 6, 6));           // helmet visor accent
}
