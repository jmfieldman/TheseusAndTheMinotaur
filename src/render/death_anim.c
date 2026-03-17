#include "death_anim.h"
#include "shader.h"
#include "game/tile_physics.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ── Physics constants ─────────────────────────────────── */

#define GRAVITY          -9.8f   /* world-units/s² (downward) */
#define FLOOR_ELASTICITY  0.35f  /* vertical bounce coefficient */
#define ANGULAR_DAMPING   0.7f   /* angular velocity multiplier on bounce */
#define REST_SPEED_THRESHOLD 0.15f /* speed below which rest timer starts */
#define REST_TIME_REQUIRED   0.1f  /* seconds at low speed before freezing */
#define PIT_VANISH_Y     -0.5f   /* Y below which pit voxels start shrinking */
#define PIT_SHRINK_RATE   3.0f   /* scale shrink per second when below vanish Y */
#define PIT_MIN_SCALE     0.01f  /* scale at which a voxel is marked fallen */

/* Forward duration — specific death types will override in Steps 6.10+ */
#define DEFAULT_DURATION  1.2f
#define REVERSE_DURATION_FACTOR 0.5f

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

        /* Direction from actor center outward */
        float dx = v->pos[0] - v->orig_pos[0];
        float dz = v->pos[2] - v->orig_pos[2];

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
 * outward along the ground with low force, no upward arc.
 */
static void apply_squish_scatter(DeathAnim* da) {
    for (int i = 0; i < da->count; i++) {
        DeathVoxel* v = &da->voxels[i];

        /* Direction from actor center outward (XZ only) */
        float dx = v->pos[0] - v->orig_pos[0];
        float dz = v->pos[2] - v->orig_pos[2];

        /* Deterministic per-voxel noise */
        unsigned int seed = (unsigned int)(i * 2654435761u);
        float rand1 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float rand2 = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;

        /* Gentle horizontal scatter — outward from center + small random */
        v->vel[0] = dx * 2.0f + rand1 * 0.6f;
        v->vel[2] = dz * 2.0f + rand2 * 0.6f;

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
    decompose_actor(da, actor, cx, cz);

    /* Apply scatter velocities based on death type */
    switch (type) {
        case DEATH_SQUISH:
            apply_squish_scatter(da);
            break;
        /* Specific death types will be implemented in Steps 6.10–6.14.
         * For now, remaining types use the generic scatter. */
        case DEATH_WALK_INTO:
        case DEATH_SPIKE:
        case DEATH_PETRIFY:
        case DEATH_PIT_FALL:
        case DEATH_GENERIC:
        default:
            apply_generic_scatter(da);
            break;
    }

    da->active = true;
    da->finished = false;
    da->reversing = false;
    da->timer = 0.0f;
}

void death_anim_update(DeathAnim* da, float dt) {
    if (!da->active || da->finished) return;

    if (da->reversing) {
        /* ── Reverse playback: lerp all voxels back to original positions ── */
        da->timer += dt;
        float t = da->timer / da->reverse_duration;
        if (t >= 1.0f) t = 1.0f;

        /* Smooth ease-in-out */
        float u = t * t * (3.0f - 2.0f * t);

        for (int i = 0; i < da->count; i++) {
            DeathVoxel* v = &da->voxels[i];

            /* Lerp from final snapshot back to original */
            for (int c = 0; c < 3; c++) {
                v->pos[c]   = da->final_pos[i][c]   + (v->orig_pos[c]   - da->final_pos[i][c]) * u;
                v->rot[c]   = da->final_rot[i][c]   * (1.0f - u);
                v->scale[c] = da->final_scale[i][c]  + (v->orig_scale[c] - da->final_scale[i][c]) * u;
            }
            for (int c = 0; c < 4; c++) {
                v->color[c] = da->final_color[i][c] + (v->orig_color[c] - da->final_color[i][c]) * u;
            }

            /* Un-fall voxels that were in pits */
            if (da->final_fallen[i]) {
                v->fallen = (u < 0.3f); /* reappear early in the reverse */
                if (!v->fallen) {
                    /* Scale back up */
                    float reappear_t = (u - 0.3f) / 0.7f;
                    float s = reappear_t < 1.0f ? reappear_t : 1.0f;
                    v->scale[0] = v->orig_scale[0] * s;
                    v->scale[1] = v->orig_scale[1] * s;
                    v->scale[2] = v->orig_scale[2] * s;
                }
            }
        }

        if (t >= 1.0f) {
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

        /* Determine which tile this voxel is on */
        int vcol = (int)floorf(v->pos[0]);
        int vrow = (int)floorf(v->pos[2]);

        TileSurface surface = tile_physics_query(da->grid, da->biome,
                                                  vcol, vrow);

        /* Pit fall-through */
        if (surface.is_pit) {
            if (v->pos[1] < PIT_VANISH_Y) {
                float shrink = PIT_SHRINK_RATE * dt;
                v->scale[0] -= shrink;
                v->scale[1] -= shrink;
                v->scale[2] -= shrink;
                if (v->scale[0] < PIT_MIN_SCALE) {
                    v->scale[0] = 0.0f;
                    v->scale[1] = 0.0f;
                    v->scale[2] = 0.0f;
                    v->fallen = true;
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
            } else {
                v->vel[1] = 0.0f;
            }
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

        /* Override vertex color via uniform.
         * The voxel shader multiplies vertex color by the fragment color,
         * but our unit cube vertices are white (1,1,1,1), so we can
         * tint via a color uniform if the shader supports it.
         * Since the voxel shader uses vertex color directly, we need to
         * update the VBO or use a per-instance approach.
         *
         * Simpler approach: use the model's vertex color as-is (white)
         * and set a color multiplier uniform. The voxel shader doesn't
         * have one, so instead we modify the light ambient to approximate.
         *
         * Actually, the cleanest approach for now: just draw with white
         * voxels tinted by the existing lighting, and set the actor
         * color via u_actor_ground_y trick... No, let's use the simplest
         * path: the shader uses vertex color from the VBO. Since all
         * voxels share one white VBO, we need a color override.
         *
         * Use u_deform_height < 0 to skip normal correction, and
         * set a flat color via the existing uniform infrastructure.
         * Actually the simplest: set u_has_ao=0, which makes the shader
         * just use vertex_color * lighting. We can't change vertex color
         * per-draw without modifying the VBO.
         *
         * Best solution for 6.9a: rebuild the cube VBO with the voxel's
         * actual color baked in. But that's wasteful for 64 draws.
         *
         * Pragmatic solution: use a tint uniform. The voxel shader
         * doesn't have one, but we can repurpose an unused uniform or
         * accept white voxels for now. Let's accept colored lighting
         * by setting the directional light color to the voxel color.
         * No, that's hacky.
         *
         * Final approach: We'll create per-color cube VBOs on demand.
         * Actually no — we'll just re-upload the color portion. No —
         * simplest: we'll use glVertexAttrib4f to set a constant vertex
         * attribute for color (location 2) when the VAO's attribute
         * array is disabled. */

        /* Disable the color vertex attribute array and set a constant value */
        glDisableVertexAttribArray(2);
        glVertexAttrib4f(2, v->color[0], v->color[1], v->color[2], v->color[3]);

        glDrawArrays(GL_TRIANGLES, 0, da->cube_vertex_count);

        /* Re-enable for next draw (or other meshes) */
        glEnableVertexAttribArray(2);
    }

    glBindVertexArray(0);
}

/* ── Reverse playback ──────────────────────────────────── */

void death_anim_start_reverse(DeathAnim* da) {
    if (!da->active) return;

    /* Snapshot current state as "end keyframe" for lerp */
    for (int i = 0; i < da->count; i++) {
        const DeathVoxel* v = &da->voxels[i];
        da->final_pos[i][0]   = v->pos[0];
        da->final_pos[i][1]   = v->pos[1];
        da->final_pos[i][2]   = v->pos[2];
        da->final_rot[i][0]   = v->rot[0];
        da->final_rot[i][1]   = v->rot[1];
        da->final_rot[i][2]   = v->rot[2];
        da->final_scale[i][0] = v->scale[0];
        da->final_scale[i][1] = v->scale[1];
        da->final_scale[i][2] = v->scale[2];
        da->final_color[i][0] = v->color[0];
        da->final_color[i][1] = v->color[1];
        da->final_color[i][2] = v->color[2];
        da->final_color[i][3] = v->color[3];
        da->final_fallen[i]   = v->fallen;
    }

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
