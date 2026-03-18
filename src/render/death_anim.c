#include "death_anim.h"
#include "shader.h"
#include "game/tile_physics.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ── Physics constants ─────────────────────────────────── */

#define GRAVITY          -9.8f   /* world-units/s² (downward) */
#define FLOOR_ELASTICITY  0.35f  /* vertical bounce coefficient */
#define WALL_ELASTICITY   0.5f   /* horizontal bounce coefficient for wall hits */
#define ANGULAR_DAMPING   0.7f   /* angular velocity multiplier on bounce */
#define REST_SPEED_THRESHOLD 0.08f /* speed below which rest timer starts */
#define REST_TIME_REQUIRED   0.3f  /* seconds at low speed before freezing */
#define PIT_VANISH_Y     -0.5f   /* Y below which pit voxels start shrinking */
#define PIT_SHRINK_RATE   3.0f   /* scale shrink per second when below vanish Y */
#define PIT_MIN_SCALE     0.01f  /* scale at which a voxel is marked fallen */

/* Forward duration — specific death types will override in Steps 6.10+ */
#define DEFAULT_DURATION  3.0f
#define REVERSE_DURATION_FACTOR 0.5f

/* Squish-specific: Y scale compression phase */
#define SQUASH_PHASE_DURATION 0.15f  /* seconds to compress Y scale */
#define SQUASH_SCALE_Y        0.2f   /* compressed Y scale target */

/* Subdivision: decompose actor body into N×N×N sub-cubes */
#define DECOMPOSE_N  4

/* ── Unit cube geometry ────────────────────────────────── */

/*
 * Vertex layout matches the voxel shader: 13 floats per vertex.
 *   position(3) + normal(3) + color(4) + uv(2) + ao_mode(1)
 *
 * We build a simple unit cube [0,1]^3 with outward normals.
 * Color/UV/AO are set to neutral values — color is overridden per-voxel
 * via the vertex color attribute in the shader (we set u_has_ao=0 and
 * the shader uses vertex color directly).
 */

#define CUBE_FLOATS_PER_VERT 13
#define CUBE_VERTS_PER_FACE  6
#define CUBE_FACES           6
#define CUBE_TOTAL_VERTS     (CUBE_FACES * CUBE_VERTS_PER_FACE)

static void emit_cube_vert(float* buf, int* idx,
                            float px, float py, float pz,
                            float nx, float ny, float nz) {
    float* dst = &buf[(*idx) * CUBE_FLOATS_PER_VERT];
    dst[0]  = px;  dst[1]  = py;  dst[2]  = pz;   /* position */
    dst[3]  = nx;  dst[4]  = ny;  dst[5]  = nz;   /* normal */
    dst[6]  = 1.0f; dst[7] = 1.0f; dst[8] = 1.0f; dst[9] = 1.0f; /* color (white) */
    dst[10] = 0.0f; dst[11] = 0.0f;  /* uv */
    dst[12] = 1.0f; /* ao_mode = ATLAS (> 0.5 → shader samples AO texture) */
    (*idx)++;
}

static void build_unit_cube(float* buf, int* vert_count) {
    int vi = 0;

    /* +Y face (top) */
    emit_cube_vert(buf, &vi, 0,1,0, 0,1,0);
    emit_cube_vert(buf, &vi, 1,1,0, 0,1,0);
    emit_cube_vert(buf, &vi, 1,1,1, 0,1,0);
    emit_cube_vert(buf, &vi, 0,1,0, 0,1,0);
    emit_cube_vert(buf, &vi, 1,1,1, 0,1,0);
    emit_cube_vert(buf, &vi, 0,1,1, 0,1,0);

    /* -Y face (bottom) */
    emit_cube_vert(buf, &vi, 0,0,1, 0,-1,0);
    emit_cube_vert(buf, &vi, 1,0,1, 0,-1,0);
    emit_cube_vert(buf, &vi, 1,0,0, 0,-1,0);
    emit_cube_vert(buf, &vi, 0,0,1, 0,-1,0);
    emit_cube_vert(buf, &vi, 1,0,0, 0,-1,0);
    emit_cube_vert(buf, &vi, 0,0,0, 0,-1,0);

    /* +X face (east) */
    emit_cube_vert(buf, &vi, 1,0,0, 1,0,0);
    emit_cube_vert(buf, &vi, 1,0,1, 1,0,0);
    emit_cube_vert(buf, &vi, 1,1,1, 1,0,0);
    emit_cube_vert(buf, &vi, 1,0,0, 1,0,0);
    emit_cube_vert(buf, &vi, 1,1,1, 1,0,0);
    emit_cube_vert(buf, &vi, 1,1,0, 1,0,0);

    /* -X face (west) */
    emit_cube_vert(buf, &vi, 0,0,1, -1,0,0);
    emit_cube_vert(buf, &vi, 0,0,0, -1,0,0);
    emit_cube_vert(buf, &vi, 0,1,0, -1,0,0);
    emit_cube_vert(buf, &vi, 0,0,1, -1,0,0);
    emit_cube_vert(buf, &vi, 0,1,0, -1,0,0);
    emit_cube_vert(buf, &vi, 0,1,1, -1,0,0);

    /* +Z face (north) */
    emit_cube_vert(buf, &vi, 0,0,1, 0,0,1);
    emit_cube_vert(buf, &vi, 1,0,1, 0,0,1);
    emit_cube_vert(buf, &vi, 1,1,1, 0,0,1);
    emit_cube_vert(buf, &vi, 0,0,1, 0,0,1);
    emit_cube_vert(buf, &vi, 1,1,1, 0,0,1);
    emit_cube_vert(buf, &vi, 0,1,1, 0,0,1);

    /* -Z face (south) */
    emit_cube_vert(buf, &vi, 1,0,0, 0,0,-1);
    emit_cube_vert(buf, &vi, 0,0,0, 0,0,-1);
    emit_cube_vert(buf, &vi, 0,1,0, 0,0,-1);
    emit_cube_vert(buf, &vi, 1,0,0, 0,0,-1);
    emit_cube_vert(buf, &vi, 0,1,0, 0,0,-1);
    emit_cube_vert(buf, &vi, 1,1,0, 0,0,-1);

    *vert_count = vi;
}

static void ensure_cube_vbo(DeathAnim* da) {
    if (da->cube_vao) return;

    float buf[CUBE_TOTAL_VERTS * CUBE_FLOATS_PER_VERT];
    int vert_count = 0;
    build_unit_cube(buf, &vert_count);

    glGenVertexArrays(1, &da->cube_vao);
    glGenBuffers(1, &da->cube_vbo);

    glBindVertexArray(da->cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, da->cube_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(vert_count * CUBE_FLOATS_PER_VERT * (int)sizeof(float)),
                 buf, GL_STATIC_DRAW);

    GLsizei stride = CUBE_FLOATS_PER_VERT * (GLsizei)sizeof(float);

    /* position (location 0) */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    /* normal (location 1) */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    /* color (location 2) */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    /* uv (location 3) */
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    /* ao_mode (location 4) */
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    da->cube_vertex_count = vert_count;
}

/* ── Keyframe helpers ──────────────────────────────────── */

static void record_keyframe(DeathVoxel* v, float time) {
    if (v->keyframe_count >= VOXEL_KEYFRAME_MAX) {
        /* Out of keyframe slots — force the voxel to rest */
        v->vel[0] = v->vel[1] = v->vel[2] = 0.0f;
        v->angular_vel[0] = v->angular_vel[1] = v->angular_vel[2] = 0.0f;
        v->at_rest = true;

        /* Overwrite last keyframe with final rest state */
        VoxelKeyframe* kf = &v->keyframes[VOXEL_KEYFRAME_MAX - 1];
        kf->time = time;
        kf->pos[0] = v->pos[0]; kf->pos[1] = v->pos[1]; kf->pos[2] = v->pos[2];
        kf->vel[0] = 0.0f; kf->vel[1] = 0.0f; kf->vel[2] = 0.0f;
        kf->rot[0] = v->rot[0]; kf->rot[1] = v->rot[1]; kf->rot[2] = v->rot[2];
        kf->angular_vel[0] = 0.0f; kf->angular_vel[1] = 0.0f; kf->angular_vel[2] = 0.0f;
        kf->scale[0] = v->scale[0]; kf->scale[1] = v->scale[1]; kf->scale[2] = v->scale[2];
        kf->fallen = v->fallen;
        return;
    }

    VoxelKeyframe* kf = &v->keyframes[v->keyframe_count++];
    kf->time = time;
    kf->pos[0] = v->pos[0]; kf->pos[1] = v->pos[1]; kf->pos[2] = v->pos[2];
    kf->vel[0] = v->vel[0]; kf->vel[1] = v->vel[1]; kf->vel[2] = v->vel[2];
    kf->rot[0] = v->rot[0]; kf->rot[1] = v->rot[1]; kf->rot[2] = v->rot[2];
    kf->angular_vel[0] = v->angular_vel[0];
    kf->angular_vel[1] = v->angular_vel[1];
    kf->angular_vel[2] = v->angular_vel[2];
    kf->scale[0] = v->scale[0]; kf->scale[1] = v->scale[1]; kf->scale[2] = v->scale[2];
    kf->fallen = v->fallen;
}

/* ── Decomposition ─────────────────────────────────────── */

/*
 * Decompose the actor body into a DECOMPOSE_N^3 grid of sub-cubes.
 * Each sub-cube becomes a DeathVoxel at the actor's world position.
 */
static void decompose_actor(DeathAnim* da,
                             const ActorParts* actor,
                             float actor_cx, float actor_cz) {
    /* Actor body is a cube of size body_height, centered at (cx, 0, cz)
     * with Y base at 0.  For Theseus: 0.45, for Minotaur: 0.65. */
    float body_size = actor->body_height;
    float half = body_size * 0.5f;
    float sub_size = body_size / (float)DECOMPOSE_N;
    float sub_half = sub_size * 0.5f;

    /* Actor body color — extract from the known constants.
     * Theseus: blue (80/255, 168/255, 251/255)
     * Minotaur: red (239/255, 34/255, 34/255)
     * We distinguish by checking has_horns. */
    float r, g, b;
    if (actor->has_horns) {
        r = 239.0f / 255.0f;
        g = 34.0f / 255.0f;
        b = 34.0f / 255.0f;
    } else {
        r = 80.0f / 255.0f;
        g = 168.0f / 255.0f;
        b = 251.0f / 255.0f;
    }

    int count = 0;
    for (int iz = 0; iz < DECOMPOSE_N && count < DEATH_VOXEL_MAX; iz++) {
        for (int iy = 0; iy < DECOMPOSE_N && count < DEATH_VOXEL_MAX; iy++) {
            for (int ix = 0; ix < DECOMPOSE_N && count < DEATH_VOXEL_MAX; ix++) {
                DeathVoxel* v = &da->voxels[count];
                memset(v, 0, sizeof(DeathVoxel));

                /* Sub-cube center relative to actor origin */
                float lx = -half + sub_size * ((float)ix + 0.5f);
                float ly = sub_size * ((float)iy + 0.5f);
                float lz = -half + sub_size * ((float)iz + 0.5f);

                v->pos[0] = actor_cx + lx;
                v->pos[1] = ly;
                v->pos[2] = actor_cz + lz;

                v->orig_pos[0] = v->pos[0];
                v->orig_pos[1] = v->pos[1];
                v->orig_pos[2] = v->pos[2];

                v->scale[0] = 1.0f;
                v->scale[1] = 1.0f;
                v->scale[2] = 1.0f;
                v->orig_scale[0] = 1.0f;
                v->orig_scale[1] = 1.0f;
                v->orig_scale[2] = 1.0f;

                v->color[0] = r;
                v->color[1] = g;
                v->color[2] = b;
                v->color[3] = 1.0f;
                v->orig_color[0] = r;
                v->orig_color[1] = g;
                v->orig_color[2] = b;
                v->orig_color[3] = 1.0f;

                v->size[0] = sub_half;
                v->size[1] = sub_half;
                v->size[2] = sub_half;

                count++;
            }
        }
    }
    da->count = count;
}

/* ── Initial velocity scatter ──────────────────────────── */

/*
 * Apply generic (DEATH_GENERIC) scatter: random outward burst.
 * Specific death types will override this in Steps 6.10–6.14.
 */
static void apply_generic_scatter(DeathAnim* da) {
    for (int i = 0; i < da->count; i++) {
        DeathVoxel* v = &da->voxels[i];

        /* Direction from actor center outward — normalize */
        float dx = v->pos[0] - da->center_x;
        float dz = v->pos[2] - da->center_z;
        float dlen = sqrtf(dx * dx + dz * dz);
        if (dlen > 0.001f) { dx /= dlen; dz /= dlen; }

        /* Add some randomness using a simple deterministic hash */
        unsigned int seed = (unsigned int)(i * 2654435761u);
        float rand1 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float rand2 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float rand3 = ((float)(seed & 0xFFFF) / 65535.0f);

        /* Horizontal scatter (outward from center + random) */
        v->vel[0] = dx * 4.0f + rand1 * 1.5f;
        v->vel[2] = dz * 4.0f + rand2 * 1.5f;

        /* Upward launch */
        v->vel[1] = 2.0f + rand3 * 3.0f;

        /* Random spin */
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[0] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 12.0f;
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[1] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 12.0f;
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[2] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 12.0f;
    }
}

/*
 * DEATH_SQUISH scatter: Minotaur rolls onto Theseus — voxels slide
 * outward along the ground with no upward arc.
 *
 * The Minotaur's approach direction biases the scatter: voxels scatter
 * more strongly AWAY from the Minotaur's roll direction.
 *
 * Velocities are stored but NOT applied during the squash phase (first
 * SQUASH_PHASE_DURATION seconds) — the update loop holds them frozen
 * while Y scale compresses, then releases them.
 */
static void apply_squish_scatter(DeathAnim* da) {
    /* Approach direction (set after init via death_anim_set_approach).
     * Defaults to (0,0) if not set — pure radial scatter. */
    float adx = da->approach_dx;
    float adz = da->approach_dz;

    /* "Away" direction = opposite of approach */
    float away_x = -adx;
    float away_z = -adz;

    for (int i = 0; i < da->count; i++) {
        DeathVoxel* v = &da->voxels[i];

        /* Radial direction from actor center outward (XZ only) — normalize */
        float rx = v->pos[0] - da->center_x;
        float rz = v->pos[2] - da->center_z;
        float rlen = sqrtf(rx * rx + rz * rz);
        if (rlen > 0.001f) { rx /= rlen; rz /= rlen; }

        /* Deterministic per-voxel noise */
        unsigned int seed = (unsigned int)(i * 2654435761u);
        float rand1 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float rand2 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;

        /* Radial outward (primary) + away-from-minotaur bias (subtle) + noise */
        v->vel[0] = rx * 2.5f + away_x * 0.8f + rand1 * 0.5f;
        v->vel[2] = rz * 2.5f + away_z * 0.8f + rand2 * 0.5f;

        /* No upward launch — squashed flat */
        v->vel[1] = 0.0f;

        /* Mild spin, mostly around Y (spinning on the floor) */
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[0] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 3.0f;
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[1] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 8.0f;
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[2] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 3.0f;
    }
}

/*
 * DEATH_WALK_INTO scatter: Theseus hops into the Minotaur and shatters
 * backward on impact.  Higher velocity than squish — like bouncing off
 * a wall.  Some upward scatter for a dramatic spray, plus lateral spread.
 *
 * approach_dx/dz points TOWARD the Minotaur (Theseus's movement direction).
 * Voxels scatter in the OPPOSITE direction (backward).
 */
static void apply_walk_into_scatter(DeathAnim* da) {
    /* Bounce-back direction = opposite of approach */
    float back_x = -da->approach_dx;
    float back_z = -da->approach_dz;

    for (int i = 0; i < da->count; i++) {
        DeathVoxel* v = &da->voxels[i];

        /* Radial direction from actor center outward — normalize */
        float rx = v->pos[0] - da->center_x;
        float rz = v->pos[2] - da->center_z;
        float rlen = sqrtf(rx * rx + rz * rz);
        if (rlen > 0.001f) { rx /= rlen; rz /= rlen; }

        /* Deterministic per-voxel noise */
        unsigned int seed = (unsigned int)(i * 2654435761u);
        float rand1 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float rand2 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float rand3 = ((float)(seed & 0xFFFF) / 65535.0f);
        seed = seed * 1664525u + 1013904223u;

        /* Primary: backward (bouncing off Minotaur)
         * Secondary: radial spread + random noise */
        v->vel[0] = back_x * 1.8f + rx * 0.8f + rand1 * 0.5f;
        v->vel[2] = back_z * 1.8f + rz * 0.8f + rand2 * 0.5f;

        /* Upward spray — moderate arc */
        v->vel[1] = 1.5f + rand3 * 1.5f;

        /* Moderate spin */
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[0] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 8.0f;
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[1] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 8.0f;
        seed = seed * 1664525u + 1013904223u;
        v->angular_vel[2] = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 8.0f;
    }
}

/* ── Wall collision ────────────────────────────────────── */

/*
 * Check and resolve wall collisions for a voxel on the XZ plane.
 * Tests all four sides of the voxel's current tile.
 * Returns true if any wall collision occurred (so a keyframe is recorded).
 */
static bool resolve_wall_collisions(DeathVoxel* v,
                                     const Grid* grid) {
    int col = (int)floorf(v->pos[0]);
    int row = (int)floorf(v->pos[2]);
    float half_x = v->size[0];
    float half_z = v->size[2];
    bool hit = false;

    /* East wall: voxel right edge vs wall plane */
    if (tile_physics_has_wall(grid, col, row, DIR_EAST)) {
        float wall_x = tile_physics_wall_coord(grid, col, row, DIR_EAST);
        if (v->pos[0] + half_x > wall_x) {
            v->pos[0] = wall_x - half_x;
            v->vel[0] = -v->vel[0] * WALL_ELASTICITY;
            /* Angular impulse from wall hit */
            v->angular_vel[1] += v->vel[2] * 2.0f;
            v->angular_vel[2] -= v->vel[0] * 1.0f;
            hit = true;
        }
    }

    /* West wall: voxel left edge vs wall plane */
    if (tile_physics_has_wall(grid, col, row, DIR_WEST)) {
        float wall_x = tile_physics_wall_coord(grid, col, row, DIR_WEST);
        if (v->pos[0] - half_x < wall_x) {
            v->pos[0] = wall_x + half_x;
            v->vel[0] = -v->vel[0] * WALL_ELASTICITY;
            v->angular_vel[1] -= v->vel[2] * 2.0f;
            v->angular_vel[2] += v->vel[0] * 1.0f;
            hit = true;
        }
    }

    /* North wall: voxel front edge vs wall plane */
    if (tile_physics_has_wall(grid, col, row, DIR_NORTH)) {
        float wall_z = tile_physics_wall_coord(grid, col, row, DIR_NORTH);
        if (v->pos[2] + half_z > wall_z) {
            v->pos[2] = wall_z - half_z;
            v->vel[2] = -v->vel[2] * WALL_ELASTICITY;
            v->angular_vel[1] -= v->vel[0] * 2.0f;
            v->angular_vel[0] += v->vel[2] * 1.0f;
            hit = true;
        }
    }

    /* South wall: voxel back edge vs wall plane */
    if (tile_physics_has_wall(grid, col, row, DIR_SOUTH)) {
        float wall_z = tile_physics_wall_coord(grid, col, row, DIR_SOUTH);
        if (v->pos[2] - half_z < wall_z) {
            v->pos[2] = wall_z + half_z;
            v->vel[2] = -v->vel[2] * WALL_ELASTICITY;
            v->angular_vel[1] += v->vel[0] * 2.0f;
            v->angular_vel[0] -= v->vel[2] * 1.0f;
            hit = true;
        }
    }

    return hit;
}

/* ── Public API ────────────────────────────────────────── */

void death_anim_init(DeathAnim* da, DeathType type,
                     const ActorParts* actor,
                     float actor_x, float actor_z,
                     const Grid* grid,
                     const BiomeConfig* biome) {
    memset(da, 0, sizeof(DeathAnim));

    da->type = type;
    da->grid = grid;
    da->biome = biome;
    da->duration = DEFAULT_DURATION;
    da->reverse_duration = da->duration * REVERSE_DURATION_FACTOR;

    /* Build shared unit-cube VBO */
    ensure_cube_vbo(da);

    /* Decompose actor into sub-cubes */
    float cx = actor_x + 0.5f;  /* tile center */
    float cz = actor_z + 0.5f;
    da->center_x = cx;
    da->center_z = cz;
    decompose_actor(da, actor, cx, cz);

    /* Apply scatter velocities based on death type */
    switch (type) {
        case DEATH_SQUISH:
            apply_squish_scatter(da);
            break;
        case DEATH_WALK_INTO:
            /* Initial scatter uses generic until set_approach provides
             * the actual direction — then re-applied in set_approach. */
            apply_walk_into_scatter(da);
            break;
        /* Specific death types will be implemented in Steps 6.12–6.14.
         * For now, remaining types use the generic scatter. */
        case DEATH_SPIKE:
        case DEATH_PETRIFY:
        case DEATH_PIT_FALL:
        case DEATH_GENERIC:
        default:
            apply_generic_scatter(da);
            break;
    }

    /* Record initial keyframe (time=0) for every voxel */
    for (int i = 0; i < da->count; i++) {
        record_keyframe(&da->voxels[i], 0.0f);
    }

    da->active = true;
    da->finished = false;
    da->reversing = false;
    da->timer = 0.0f;
}

void death_anim_update(DeathAnim* da, float dt) {
    if (!da->active || da->finished) return;

    if (da->reversing) {
        /* ── Reverse playback: walk keyframes backward ── */
        da->timer += dt;
        float t = da->timer / da->reverse_duration;
        if (t >= 1.0f) t = 1.0f;

        /* Smooth ease-in-out */
        float u = t * t * (3.0f - 2.0f * t);

        for (int i = 0; i < da->count; i++) {
            DeathVoxel* v = &da->voxels[i];
            int kc = v->keyframe_count;
            if (kc < 1) continue;

            /* Map u (0→1) to keyframe time, going backward:
             * u=0 → last keyframe time, u=1 → time 0 (initial state) */
            float last_time = v->keyframes[kc - 1].time;
            float target_time = last_time * (1.0f - u);

            /* Find the two keyframes that bracket target_time */
            int ki = 0;
            for (int k = 0; k < kc - 1; k++) {
                if (v->keyframes[k + 1].time > target_time) {
                    ki = k;
                    break;
                }
                ki = k;
            }

            const VoxelKeyframe* kf_a = &v->keyframes[ki];
            const VoxelKeyframe* kf_b = (ki + 1 < kc) ? &v->keyframes[ki + 1] : kf_a;

            /* Interpolation factor between the two keyframes */
            float seg_len = kf_b->time - kf_a->time;
            float seg_t = (seg_len > 0.0001f)
                        ? (target_time - kf_a->time) / seg_len
                        : 0.0f;
            if (seg_t < 0.0f) seg_t = 0.0f;
            if (seg_t > 1.0f) seg_t = 1.0f;

            /* Lerp position and rotation between keyframes.
             * Simple lerp is used instead of physics replay because the
             * forward simulation includes continuous friction, which makes
             * ballistic replay inaccurate between keyframes. */
            v->pos[0] = kf_a->pos[0] + (kf_b->pos[0] - kf_a->pos[0]) * seg_t;
            v->pos[1] = kf_a->pos[1] + (kf_b->pos[1] - kf_a->pos[1]) * seg_t;
            v->pos[2] = kf_a->pos[2] + (kf_b->pos[2] - kf_a->pos[2]) * seg_t;

            v->rot[0] = kf_a->rot[0] + (kf_b->rot[0] - kf_a->rot[0]) * seg_t;
            v->rot[1] = kf_a->rot[1] + (kf_b->rot[1] - kf_a->rot[1]) * seg_t;
            v->rot[2] = kf_a->rot[2] + (kf_b->rot[2] - kf_a->rot[2]) * seg_t;

            /* Scale: lerp between keyframes */
            for (int c = 0; c < 3; c++) {
                v->scale[c] = kf_a->scale[c] + (kf_b->scale[c] - kf_a->scale[c]) * seg_t;
            }

            /* Handle fallen voxels: reappear as we reverse past pit entry */
            if (kf_b->fallen && !kf_a->fallen) {
                /* We're crossing the pit-entry boundary */
                v->fallen = (seg_t > 0.5f);
                if (!v->fallen) {
                    /* Scale back up */
                    float reappear_t = (0.5f - seg_t) / 0.5f;
                    float s = 1.0f - reappear_t;
                    if (s < 0.0f) s = 0.0f;
                    v->scale[0] = v->orig_scale[0] * s;
                    v->scale[1] = v->orig_scale[1] * s;
                    v->scale[2] = v->orig_scale[2] * s;
                }
            } else {
                v->fallen = kf_a->fallen;
            }
        }

        if (t >= 1.0f) {
            /* Snap all voxels to original positions */
            for (int i = 0; i < da->count; i++) {
                DeathVoxel* v = &da->voxels[i];
                v->pos[0] = v->orig_pos[0];
                v->pos[1] = v->orig_pos[1];
                v->pos[2] = v->orig_pos[2];
                v->rot[0] = v->rot[1] = v->rot[2] = 0.0f;
                v->scale[0] = v->orig_scale[0];
                v->scale[1] = v->orig_scale[1];
                v->scale[2] = v->orig_scale[2];
                v->fallen = false;
            }
            da->finished = true;
            da->active = false;
        }
        return;
    }

    /* ── Forward playback: physics simulation ──────────── */
    da->timer += dt;

    int all_settled = 1;

    for (int i = 0; i < da->count; i++) {
        DeathVoxel* v = &da->voxels[i];
        if (v->fallen || v->at_rest) continue;

        all_settled = 0;

        /* Gravity */
        v->vel[1] += GRAVITY * dt;

        /* Integration */
        v->pos[0] += v->vel[0] * dt;
        v->pos[1] += v->vel[1] * dt;
        v->pos[2] += v->vel[2] * dt;
        v->rot[0] += v->angular_vel[0] * dt;
        v->rot[1] += v->angular_vel[1] * dt;
        v->rot[2] += v->angular_vel[2] * dt;

        /* Wall collision — check and resolve, record keyframe on hit */
        if (resolve_wall_collisions(v, da->grid)) {
            record_keyframe(v, da->timer);
        }

        /* Determine which tile this voxel is on (may have changed after
         * wall collision pushed it back) */
        int vcol = (int)floorf(v->pos[0]);
        int vrow = (int)floorf(v->pos[2]);

        TileSurface surface = tile_physics_query(da->grid, da->biome,
                                                  vcol, vrow);

        /* Pit fall-through */
        if (surface.is_pit) {
            if (v->pos[1] < PIT_VANISH_Y) {
                /* Record keyframe at pit entry (first time crossing) */
                if (v->scale[0] > 1.0f - 0.01f) {
                    record_keyframe(v, da->timer);
                }

                float shrink = PIT_SHRINK_RATE * dt;
                v->scale[0] -= shrink;
                v->scale[1] -= shrink;
                v->scale[2] -= shrink;
                if (v->scale[0] < PIT_MIN_SCALE) {
                    v->scale[0] = 0.0f;
                    v->scale[1] = 0.0f;
                    v->scale[2] = 0.0f;
                    v->fallen = true;
                    record_keyframe(v, da->timer);
                }
            }
            /* No floor clamping — voxel falls freely */
            continue;
        }

        /* Floor clamping — voxel bottom is at pos[1] - size[1] */
        float voxel_bottom = v->pos[1] - v->size[1];
        if (voxel_bottom <= surface.surface_y) {
            v->pos[1] = surface.surface_y + v->size[1];

            /* Bounce */
            if (v->vel[1] < -0.1f) {
                v->vel[1] = -v->vel[1] * FLOOR_ELASTICITY;

                /* Dampen angular velocity on ground contact */
                v->angular_vel[0] *= ANGULAR_DAMPING;
                v->angular_vel[1] *= ANGULAR_DAMPING;
                v->angular_vel[2] *= ANGULAR_DAMPING;

                /* Friction: dampen horizontal velocity on each bounce */
                v->vel[0] *= 0.85f;
                v->vel[2] *= 0.85f;

                /* Record keyframe at floor bounce */
                record_keyframe(v, da->timer);
            } else {
                v->vel[1] = 0.0f;
            }
        }

        /* Ground friction: continuously dampen horizontal velocity while
         * on or near the floor.  Uses exponential decay so the result is
         * framerate-independent (same distance regardless of dt). */
        if (voxel_bottom <= surface.surface_y + 0.02f) {
            float decay = expf(-3.0f * dt);  /* half-life ≈ 0.23s */
            v->vel[0] *= decay;
            v->vel[2] *= decay;
            v->angular_vel[0] *= decay;
            v->angular_vel[1] *= decay;
            v->angular_vel[2] *= decay;
        }

        /* Periodic keyframe sampling for smooth reverse playback.
         * Since we use position lerp (not physics replay) during reverse,
         * we need keyframes at regular intervals to trace the path. */
        #define KEYFRAME_INTERVAL 0.1f  /* seconds between periodic samples */
        if (da->timer - v->last_keyframe_time >= KEYFRAME_INTERVAL) {
            record_keyframe(v, da->timer);
            v->last_keyframe_time = da->timer;
        }

        /* Rest detection */
        float speed_sq = v->vel[0] * v->vel[0] +
                         v->vel[1] * v->vel[1] +
                         v->vel[2] * v->vel[2];
        float ang_speed_sq = v->angular_vel[0] * v->angular_vel[0] +
                             v->angular_vel[1] * v->angular_vel[1] +
                             v->angular_vel[2] * v->angular_vel[2];
        float total_speed = sqrtf(speed_sq) + sqrtf(ang_speed_sq) * 0.1f;

        if (total_speed < REST_SPEED_THRESHOLD &&
            voxel_bottom <= surface.surface_y + 0.01f) {
            v->rest_timer += dt;
            if (v->rest_timer >= REST_TIME_REQUIRED) {
                v->at_rest = true;
                v->vel[0] = v->vel[1] = v->vel[2] = 0.0f;
                v->angular_vel[0] = v->angular_vel[1] = v->angular_vel[2] = 0.0f;
                record_keyframe(v, da->timer);
            }
        } else {
            v->rest_timer = 0.0f;
        }
    }

    /* End forward playback when all voxels have settled or timer expires */
    if (all_settled || da->timer >= da->duration) {
        /* Stop any remaining voxels */
        for (int i = 0; i < da->count; i++) {
            DeathVoxel* v = &da->voxels[i];
            if (!v->fallen && !v->at_rest) {
                v->at_rest = true;
                v->vel[0] = v->vel[1] = v->vel[2] = 0.0f;
                v->angular_vel[0] = v->angular_vel[1] = v->angular_vel[2] = 0.0f;
                record_keyframe(v, da->timer);
            }
        }
        da->finished = true;
    }
}

/* ── Rendering ─────────────────────────────────────────── */

void death_anim_render(const DeathAnim* da, GLuint shader) {
    if (!da->active || da->count == 0 || !da->cube_vao) return;

    /* Disable deformation for rigid voxels */
    shader_set_float(shader, "u_deform_height", 0.0f);
    shader_set_int(shader, "u_has_ao", 0);
    shader_set_float(shader, "u_ao_intensity", 1.0f);

    glBindVertexArray(da->cube_vao);

    for (int i = 0; i < da->count; i++) {
        const DeathVoxel* v = &da->voxels[i];
        if (v->fallen) continue;
        if (v->scale[0] < PIT_MIN_SCALE) continue;

        /* Build model matrix: translate → rotate → scale.
         * The unit cube is [0,1]^3, so we scale by size*2 (full extent)
         * and offset by -size to center it. */
        float sx = v->size[0] * 2.0f * v->scale[0];
        float sy = v->size[1] * 2.0f * v->scale[1];
        float sz = v->size[2] * 2.0f * v->scale[2];

        /* Rotation (Euler XYZ — good enough for tumbling voxels) */
        float cx = cosf(v->rot[0]), csx = sinf(v->rot[0]);
        float cy = cosf(v->rot[1]), csy = sinf(v->rot[1]);
        float cz = cosf(v->rot[2]), csz = sinf(v->rot[2]);

        /* Combined rotation matrix R = Rz * Ry * Rx */
        float r00 = cy * cz;
        float r01 = csx * csy * cz - cx * csz;
        float r02 = cx * csy * cz + csx * csz;
        float r10 = cy * csz;
        float r11 = csx * csy * csz + cx * cz;
        float r12 = cx * csy * csz - csx * cz;
        float r20 = -csy;
        float r21 = csx * cy;
        float r22 = cx * cy;

        /* Model = Translate(pos) * Rotate * Scale * Translate(-0.5,-0.5,-0.5)
         * The last translate centers the unit cube at the origin before
         * scaling, so the voxel rotates around its center. */
        float model[16];
        memset(model, 0, sizeof(model));

        /* Scale + rotation combined */
        model[0]  = r00 * sx;  model[1]  = r10 * sx;  model[2]  = r20 * sx;
        model[4]  = r01 * sy;  model[5]  = r11 * sy;  model[6]  = r21 * sy;
        model[8]  = r02 * sz;  model[9]  = r12 * sz;  model[10] = r22 * sz;
        model[15] = 1.0f;

        /* Translation: pos - R*S*(0.5,0.5,0.5)
         * This accounts for the unit cube offset. */
        float hsx = sx * 0.5f, hsy = sy * 0.5f, hsz = sz * 0.5f;
        model[12] = v->pos[0] - (r00 * hsx + r01 * hsy + r02 * hsz);
        model[13] = v->pos[1] - (r10 * hsx + r11 * hsy + r12 * hsz);
        model[14] = v->pos[2] - (r20 * hsx + r21 * hsy + r22 * hsz);

        shader_set_mat4(shader, "u_model", model);

        /* Disable the color vertex attribute array and set a constant value */
        glDisableVertexAttribArray(2);
        glVertexAttrib4f(2, v->color[0], v->color[1], v->color[2], v->color[3]);

        glDrawArrays(GL_TRIANGLES, 0, da->cube_vertex_count);

        /* Re-enable for next draw (or other meshes) */
        glEnableVertexAttribArray(2);
    }

    glBindVertexArray(0);
}

/* ── Approach direction ─────────────────────────────────── */

void death_anim_set_approach(DeathAnim* da, float dx, float dz) {
    /* Normalize */
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.001f) {
        da->approach_dx = dx / len;
        da->approach_dz = dz / len;
    } else {
        da->approach_dx = 0.0f;
        da->approach_dz = 0.0f;
    }

    /* Re-apply scatter with the now-known direction.
     * This is safe because init already applied scatter with
     * approach=(0,0), and we haven't started updating yet. */
    if (da->type == DEATH_SQUISH) {
        apply_squish_scatter(da);
    } else if (da->type == DEATH_WALK_INTO) {
        apply_walk_into_scatter(da);
    }
}

/* ── Reverse playback ──────────────────────────────────── */

void death_anim_start_reverse(DeathAnim* da) {
    if (!da->active) return;

    da->reversing = true;
    da->finished = false;
    da->timer = 0.0f;
}

/* ── Queries ───────────────────────────────────────────── */

bool death_anim_is_finished(const DeathAnim* da) {
    return da->finished;
}

bool death_anim_is_active(const DeathAnim* da) {
    return da->active;
}

/* ── Cleanup ───────────────────────────────────────────── */

void death_anim_destroy(DeathAnim* da) {
    if (da->cube_vao) {
        glDeleteVertexArrays(1, &da->cube_vao);
        da->cube_vao = 0;
    }
    if (da->cube_vbo) {
        glDeleteBuffers(1, &da->cube_vbo);
        da->cube_vbo = 0;
    }
    da->active = false;
    da->count = 0;
}
