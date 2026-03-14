#include "render/diorama_gen.h"

#include "engine/utils.h"
#include "game/feature.h"

#include <math.h>
#include <string.h>

/* ---------- Constants ---------- */

#define WALL_HEIGHT       0.30f
#define WALL_THICKNESS    0.20f
#define FLOOR_THICKNESS   0.15f

/* ---------- Seeded RNG (xorshift32) ---------- */

typedef struct {
    uint32_t state;
} RNG;

static void rng_seed(RNG* rng, const char* level_id) {
    /* djb2 hash */
    uint32_t h = 5381;
    for (const char* p = level_id; *p; p++) {
        h = ((h << 5) + h) + (uint32_t)*p;
    }
    if (h == 0) h = 1;
    rng->state = h;
}

static uint32_t rng_next(RNG* rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

/* Returns float in [0, 1) */
static float rng_float(RNG* rng) {
    return (float)(rng_next(rng) & 0x7FFFFF) / (float)0x800000;
}

/* Returns float in [-range, +range] */
static float rng_jitter(RNG* rng, float range) {
    return (rng_float(rng) * 2.0f - 1.0f) * range;
}

/* Returns int in [0, max) */
static int rng_int(RNG* rng, int max) {
    if (max <= 0) return 0;
    return (int)(rng_next(rng) % (uint32_t)max);
}

/* ---------- Helpers ---------- */

static void add_box(VoxelMesh* mesh, float x, float y, float z,
                    float sx, float sy, float sz,
                    float r, float g, float b, float a,
                    bool no_cull) {
    voxel_mesh_add_box_ex(mesh, x, y, z, sx, sy, sz, r, g, b, a, no_cull, AO_MODE_ATLAS);
}

static void add_box_ao(VoxelMesh* mesh, float x, float y, float z,
                        float sx, float sy, float sz,
                        float r, float g, float b, float a,
                        bool no_cull, AoMode ao_mode) {
    voxel_mesh_add_box_ex(mesh, x, y, z, sx, sy, sz, r, g, b, a, no_cull, ao_mode);
}

static void add_light(DioramaGenResult* result,
                      float x, float y, float z,
                      float r, float g, float b,
                      float radius) {
    if (result->light_count >= LIGHTING_MAX_POINT_LIGHTS) return;
    PointLight* pl = &result->lights[result->light_count++];
    pl->pos[0] = x; pl->pos[1] = y; pl->pos[2] = z;
    pl->color[0] = r; pl->color[1] = g; pl->color[2] = b;
    pl->radius = radius;
}

/* Check if a wall segment is a door (entrance or exit) */
static bool is_door(const Grid* grid, int col, int row, Direction side) {
    if (col == grid->entrance_col && row == grid->entrance_row &&
        grid->entrance_side == side)
        return true;
    if (col == grid->exit_col && row == grid->exit_row &&
        grid->exit_side == side)
        return true;
    return false;
}

/* ---------- Floor tiles ---------- */

static void gen_floor(VoxelMesh* mesh, const Grid* grid,
                      const BiomeConfig* biome, RNG* rng) {
    for (int r = 0; r < grid->rows; r++) {
        for (int c = 0; c < grid->cols; c++) {
            const Cell* cell = grid_cell_const(grid, c, r);
            if (cell->impassable) continue;

            /* Checkerboard: one flat box per logical tile */
            const float* base_color;
            if ((c + r) % 2 == 0) {
                base_color = biome->palette.floor_a;
            } else {
                base_color = biome->palette.floor_b;
            }

            /* Very subtle per-tile color jitter for natural variation */
            float cj = rng_jitter(rng, biome->floor_style.color_jitter);

            add_box_ao(mesh,
                       (float)c, -FLOOR_THICKNESS, (float)r,
                       1.0f, FLOOR_THICKNESS, 1.0f,
                       base_color[0] + cj,
                       base_color[1] + cj,
                       base_color[2] + cj,
                       1.0f, false, AO_MODE_LIGHTMAP);
        }
    }
}

/* ---------- Step 3: Walls ---------- */

/*
 * Wall deduplication:
 *   - North/East walls: always emit
 *   - South walls: only at row 0
 *   - West walls: only at col 0
 *   - Skip entrance/exit door segments
 */

/* Emit a single wall prism for one grid edge.
 * Stone block detail (mortar lines, beveled edges, per-stone color variation,
 * surface grain) is handled procedurally in the fragment shader — no need for
 * multiple geometry blocks per edge. */
static void emit_wall_segment(VoxelMesh* mesh, const BiomeConfig* biome,
                               RNG* rng,
                               float base_x, float base_z,
                               float seg_len_x, float seg_len_z,
                               WallOrient orient) {
    (void)rng;  /* no longer needed — all variation is in the shader */

    voxel_mesh_add_wall(mesh, base_x, 0.0f, base_z,
                        seg_len_x, WALL_HEIGHT, seg_len_z,
                        biome->palette.wall[0],
                        biome->palette.wall[1],
                        biome->palette.wall[2],
                        1.0f, true, orient);
}

/*
 * Wall edge / corner helpers.
 *
 * Grid vertices sit at integer coordinates (vx, vz) where
 *   0 ≤ vx ≤ cols,  0 ≤ vz ≤ rows.
 *
 * A "horizontal edge" from (vx,vz) to (vx+1,vz) corresponds to
 *   the north wall of cell (vx, vz-1)  [if vz > 0]
 *   or the south wall of cell (vx, vz)  [if vz == 0].
 *
 * A "vertical edge" from (vx,vz) to (vx,vz+1) corresponds to
 *   the east wall of cell (vx-1, vz)  [if vx > 0]
 *   or the west wall of cell (vx, vz)  [if vx == 0].
 */

/* True if a non-door horizontal wall exists at z=vz from x=vx to x=vx+1 */
static bool has_h_wall(const Grid* grid, int vx, int vz) {
    if (vx < 0 || vx >= grid->cols) return false;
    if (vz > 0 && vz <= grid->rows) {
        int r = vz - 1;
        return grid_has_wall(grid, vx, r, DIR_NORTH) &&
               !is_door(grid, vx, r, DIR_NORTH);
    }
    if (vz == 0) {
        return grid_has_wall(grid, vx, 0, DIR_SOUTH) &&
               !is_door(grid, vx, 0, DIR_SOUTH);
    }
    return false;
}

/* True if a non-door vertical wall exists at x=vx from z=vz to z=vz+1 */
static bool has_v_wall(const Grid* grid, int vx, int vz) {
    if (vz < 0 || vz >= grid->rows) return false;
    if (vx > 0 && vx <= grid->cols) {
        int c = vx - 1;
        return grid_has_wall(grid, c, vz, DIR_EAST) &&
               !is_door(grid, c, vz, DIR_EAST);
    }
    if (vx == 0) {
        return grid_has_wall(grid, 0, vz, DIR_WEST) &&
               !is_door(grid, 0, vz, DIR_WEST);
    }
    return false;
}

/* True if vertex (vx,vz) is a corner where perpendicular walls meet.
 * This includes L-corners, T-junctions, and crossings. */
static bool is_wall_corner(const Grid* grid, int vx, int vz) {
    bool has_horiz = has_h_wall(grid, vx, vz) || has_h_wall(grid, vx - 1, vz);
    bool has_vert  = has_v_wall(grid, vx, vz) || has_v_wall(grid, vx, vz - 1);
    return has_horiz && has_vert;
}

static void gen_walls(VoxelMesh* mesh, const Grid* grid,
                      const BiomeConfig* biome, RNG* rng) {
    float half_t = WALL_THICKNESS * 0.5f;
    int cols = grid->cols;
    int rows = grid->rows;

    /* --- Pass 1: Corner blocks at every vertex where walls meet at 90° --- */
    for (int vz = 0; vz <= rows; vz++) {
        for (int vx = 0; vx <= cols; vx++) {
            if (!is_wall_corner(grid, vx, vz)) continue;

            voxel_mesh_add_wall(mesh,
                       (float)vx - half_t, 0.0f, (float)vz - half_t,
                       WALL_THICKNESS, WALL_HEIGHT, WALL_THICKNESS,
                       biome->palette.wall[0],
                       biome->palette.wall[1],
                       biome->palette.wall[2],
                       1.0f, true, WALL_ORIENT_CORNER);
        }
    }

    /* Small overlap so wall segments extend slightly into corner blocks,
     * preventing z-fighting at coplanar boundaries. */
    const float OVERLAP = 0.002f;

    /* --- Pass 2: Horizontal wall segments (trimmed around corners) --- */
    for (int vz = 0; vz <= rows; vz++) {
        for (int vx = 0; vx < cols; vx++) {
            if (!has_h_wall(grid, vx, vz)) continue;

            bool corner_left  = is_wall_corner(grid, vx, vz);
            bool corner_right = is_wall_corner(grid, vx + 1, vz);

            float start_x = (float)vx + (corner_left  ? (half_t - OVERLAP) : 0.0f);
            float end_x   = (float)(vx + 1) - (corner_right ? (half_t - OVERLAP) : 0.0f);
            float seg_len = end_x - start_x;

            if (seg_len <= 0.001f) continue; /* fully covered by corners */

            emit_wall_segment(mesh, biome, rng,
                              start_x, (float)vz - half_t,
                              seg_len, WALL_THICKNESS, WALL_ORIENT_H);
        }
    }

    /* --- Pass 3: Vertical wall segments (trimmed around corners) --- */
    for (int vx = 0; vx <= cols; vx++) {
        for (int vz = 0; vz < rows; vz++) {
            if (!has_v_wall(grid, vx, vz)) continue;

            bool corner_bottom = is_wall_corner(grid, vx, vz);
            bool corner_top    = is_wall_corner(grid, vx, vz + 1);

            float start_z = (float)vz + (corner_bottom ? (half_t - OVERLAP) : 0.0f);
            float end_z   = (float)(vz + 1) - (corner_top ? (half_t - OVERLAP) : 0.0f);
            float seg_len = end_z - start_z;

            if (seg_len <= 0.001f) continue;

            emit_wall_segment(mesh, biome, rng,
                              (float)vx - half_t, start_z,
                              WALL_THICKNESS, seg_len, WALL_ORIENT_V);
        }
    }
}

/* ---------- Step 4: Back wall ---------- */

static void gen_back_wall(VoxelMesh* mesh, const Grid* grid,
                          const BiomeConfig* biome, RNG* rng) {
    float bw_height = WALL_HEIGHT * biome->back_wall.height_multiplier;
    float half_t = WALL_THICKNESS * 0.5f;

    /* Back wall runs along the north edge of the grid.
     * Skip columns that already have a north wall segment — those are
     * generated by gen_walls() with AO_MODE_NONE and would z-fight with
     * these AO_MODE_ATLAS boxes at the exact same position. */
    for (int c = 0; c < grid->cols; c++) {
        if (has_h_wall(grid, c, grid->rows)) continue;

        add_box(mesh, (float)c, 0.0f, (float)grid->rows - half_t,
                1.0f, bw_height, WALL_THICKNESS,
                biome->palette.back_wall[0],
                biome->palette.back_wall[1],
                biome->palette.back_wall[2],
                1.0f, true);
    }

    /* Decorations on back wall */
    if (biome->back_wall.decoration_density > 0.0f &&
        biome->wall_decorations.prefab_count > 0) {
        for (int c = 0; c < grid->cols; c++) {
            if (rng_float(rng) > biome->back_wall.decoration_density) continue;

            int pi = rng_int(rng, biome->wall_decorations.prefab_count);
            const char* pname = biome->wall_decorations.prefab_names[pi];
            const BiomePrefab* prefab = biome_config_find_prefab(biome, pname);
            if (!prefab) continue;

            float ox = (float)c + 0.5f;
            float oy = WALL_HEIGHT * 0.5f;
            float oz = (float)grid->rows;

            for (int bi = 0; bi < prefab->box_count; bi++) {
                const PrefabBox* pb = &prefab->boxes[bi];
                add_box(mesh,
                        ox + pb->dx - pb->sx * 0.5f,
                        oy + pb->dy,
                        oz + pb->dz - pb->sz * 0.5f,
                        pb->sx, pb->sy, pb->sz,
                        pb->r, pb->g, pb->b, pb->a,
                        pb->no_cull);
            }
        }
    }
}

/* ---------- Step 5: Doors ---------- */

static void gen_doors(VoxelMesh* mesh, const Grid* grid,
                      const BiomeConfig* biome) {
    float frame_h = (float)biome->doors.frame_height_blocks * 0.1f;
    float pillar_w = WALL_THICKNESS;

    /* Helper: emit door frame (two pillars + lintel) */
    struct { int col, row; Direction side; } doors[2] = {
        { grid->entrance_col, grid->entrance_row, grid->entrance_side },
        { grid->exit_col,     grid->exit_row,     grid->exit_side     }
    };

    for (int d = 0; d < 2; d++) {
        int dc = doors[d].col;
        int dr = doors[d].row;
        Direction ds = doors[d].side;

        float cx, cz;    /* center of door opening */
        float p1x, p1z;  /* pillar 1 position */
        float p2x, p2z;  /* pillar 2 position */
        float lx, lz;    /* lintel position */
        float lsx, lsz;  /* lintel size x,z */

        float half_t = WALL_THICKNESS * 0.5f;

        if (ds == DIR_NORTH || ds == DIR_SOUTH) {
            cz = (ds == DIR_NORTH) ? (float)(dr + 1) : (float)dr;
            cx = (float)dc + 0.5f;

            p1x = (float)dc;
            p1z = cz - half_t;
            p2x = (float)dc + 1.0f - pillar_w;
            p2z = cz - half_t;

            lx = (float)dc;
            lz = cz - half_t;
            lsx = 1.0f;
            lsz = WALL_THICKNESS;
        } else {
            cx = (ds == DIR_EAST) ? (float)(dc + 1) : (float)dc;
            cz = (float)dr + 0.5f;

            p1x = cx - half_t;
            p1z = (float)dr;
            p2x = cx - half_t;
            p2z = (float)dr + 1.0f - pillar_w;

            lx = cx - half_t;
            lz = (float)dr;
            lsx = WALL_THICKNESS;
            lsz = 1.0f;
        }

        /* Pillars */
        add_box(mesh, p1x, 0.0f, p1z,
                pillar_w, frame_h, pillar_w,
                biome->palette.wall[0] * 0.9f,
                biome->palette.wall[1] * 0.9f,
                biome->palette.wall[2] * 0.9f,
                1.0f, true);

        add_box(mesh, p2x, 0.0f, p2z,
                pillar_w, frame_h, pillar_w,
                biome->palette.wall[0] * 0.9f,
                biome->palette.wall[1] * 0.9f,
                biome->palette.wall[2] * 0.9f,
                1.0f, true);

        /* Lintel */
        float lintel_h = 0.06f;
        add_box(mesh, lx, frame_h - lintel_h, lz,
                lsx, lintel_h, lsz,
                biome->palette.wall[0] * 0.85f,
                biome->palette.wall[1] * 0.85f,
                biome->palette.wall[2] * 0.85f,
                1.0f, true);
    }

    /* Exit floor tile — golden inlay */
    float inset = 0.15f;
    add_box(mesh,
            (float)grid->exit_col + inset,
            0.0f,
            (float)grid->exit_row + inset,
            1.0f - 2.0f * inset, 0.02f, 1.0f - 2.0f * inset,
            biome->palette.accent[0],
            biome->palette.accent[1],
            biome->palette.accent[2],
            1.0f, false);

    /* Exit finish border — thin outline */
    float border = 0.04f;
    float eb_y = 0.005f;
    int ec = grid->exit_col;
    int er = grid->exit_row;
    /* North strip */
    add_box(mesh, (float)ec + inset, eb_y, (float)er + 1.0f - inset - border,
            1.0f - 2.0f * inset, 0.015f, border,
            biome->palette.accent[0] * 1.2f,
            biome->palette.accent[1] * 1.1f,
            biome->palette.accent[2] * 0.8f,
            1.0f, false);
    /* South strip */
    add_box(mesh, (float)ec + inset, eb_y, (float)er + inset,
            1.0f - 2.0f * inset, 0.015f, border,
            biome->palette.accent[0] * 1.2f,
            biome->palette.accent[1] * 1.1f,
            biome->palette.accent[2] * 0.8f,
            1.0f, false);
    /* East strip */
    add_box(mesh, (float)ec + 1.0f - inset - border, eb_y, (float)er + inset,
            border, 0.015f, 1.0f - 2.0f * inset,
            biome->palette.accent[0] * 1.2f,
            biome->palette.accent[1] * 1.1f,
            biome->palette.accent[2] * 0.8f,
            1.0f, false);
    /* West strip */
    add_box(mesh, (float)ec + inset, eb_y, (float)er + inset,
            border, 0.015f, 1.0f - 2.0f * inset,
            biome->palette.accent[0] * 1.2f,
            biome->palette.accent[1] * 1.1f,
            biome->palette.accent[2] * 0.8f,
            1.0f, false);
}

/* ---------- Step 6: Impassable cells ---------- */

static void gen_impassable(VoxelMesh* mesh, const Grid* grid,
                           const BiomeConfig* biome, RNG* rng) {
    for (int r = 0; r < grid->rows; r++) {
        for (int c = 0; c < grid->cols; c++) {
            const Cell* cell = grid_cell_const(grid, c, r);
            if (!cell->impassable) continue;

            /* Try to use a prefab for visual variety */
            bool used_prefab = false;
            if (biome->floor_decorations.prefab_count > 0 && rng_float(rng) > 0.3f) {
                int pi = rng_int(rng, biome->floor_decorations.prefab_count);
                const char* pname = biome->floor_decorations.prefab_names[pi];
                const BiomePrefab* prefab = biome_config_find_prefab(biome, pname);
                if (prefab && prefab->box_count > 0) {
                    int rot = rng_int(rng, 4);
                    float ox = (float)c + 0.5f;
                    float oz = (float)r + 0.5f;

                    for (int bi = 0; bi < prefab->box_count; bi++) {
                        const PrefabBox* pb = &prefab->boxes[bi];
                        float dx = pb->dx, dz = pb->dz;
                        float sx = pb->sx, sz = pb->sz;

                        /* Rotate 90 degrees per rot step */
                        for (int ri = 0; ri < rot; ri++) {
                            float tmp = dx;
                            dx = -dz; dz = tmp;
                            tmp = sx; sx = sz; sz = tmp;
                        }

                        add_box(mesh,
                                ox + dx - sx * 0.5f,
                                pb->dy,
                                oz + dz - sz * 0.5f,
                                sx, pb->sy, sz,
                                pb->r, pb->g, pb->b, pb->a,
                                pb->no_cull);
                    }
                    used_prefab = true;
                }
            }

            if (!used_prefab) {
                /* Solid block fallback */
                add_box(mesh,
                        (float)c, -FLOOR_THICKNESS, (float)r,
                        1.0f, FLOOR_THICKNESS + WALL_HEIGHT * 1.5f, 1.0f,
                        biome->palette.impassable[0],
                        biome->palette.impassable[1],
                        biome->palette.impassable[2],
                        1.0f, false);
            }
        }
    }
}

/* ---------- Step 7: Feature markers ---------- */

static void gen_features(VoxelMesh* mesh, const Grid* grid,
                         const BiomeConfig* biome) {
    for (int r = 0; r < grid->rows; r++) {
        for (int c = 0; c < grid->cols; c++) {
            const Cell* cell = grid_cell_const(grid, c, r);

            for (int fi = 0; fi < cell->feature_count; fi++) {
                const Feature* feat = cell->features[fi];
                if (!feat || !feat->vt || !feat->vt->name) continue;

                const char* name = feat->vt->name;
                float fx = (float)c;
                float fz = (float)r;

                if (strcmp(name, "spike_trap") == 0) {
                    /* Recessed floor panel with thin slit gaps */
                    float inset = 0.1f;
                    add_box(mesh, fx + inset, -0.02f, fz + inset,
                            1.0f - 2.0f * inset, 0.02f, 1.0f - 2.0f * inset,
                            0.35f, 0.30f, 0.25f, 1.0f, false);
                    /* Slit gaps */
                    for (int s = 0; s < 3; s++) {
                        float sx = fx + 0.25f + (float)s * 0.25f;
                        add_box(mesh, sx - 0.01f, 0.0f, fz + 0.2f,
                                0.02f, 0.015f, 0.6f,
                                0.15f, 0.12f, 0.10f, 1.0f, true);
                    }
                } else if (strcmp(name, "pressure_plate") == 0) {
                    /* Slightly depressed plate with border rim */
                    float inset = 0.15f;
                    add_box(mesh, fx + inset, -0.01f, fz + inset,
                            1.0f - 2.0f * inset, 0.01f, 1.0f - 2.0f * inset,
                            0.30f, 0.50f, 0.70f, 1.0f, false);
                    /* Border rim */
                    float rim = 0.03f;
                    add_box(mesh, fx + inset, 0.0f, fz + inset,
                            1.0f - 2.0f * inset, 0.015f, rim,
                            0.25f, 0.40f, 0.60f, 1.0f, false);
                    add_box(mesh, fx + inset, 0.0f, fz + 1.0f - inset - rim,
                            1.0f - 2.0f * inset, 0.015f, rim,
                            0.25f, 0.40f, 0.60f, 1.0f, false);
                } else if (strcmp(name, "teleporter") == 0) {
                    /* Raised ring of small boxes */
                    for (int ti = 0; ti < 8; ti++) {
                        float angle = (float)ti * (float)M_PI * 0.25f;
                        float tr = 0.35f;
                        float tx = fx + 0.5f + cosf(angle) * tr - 0.04f;
                        float tz = fz + 0.5f + sinf(angle) * tr - 0.04f;
                        add_box(mesh, tx, 0.0f, tz,
                                0.08f, 0.04f, 0.08f,
                                0.55f, 0.30f, 0.70f, 1.0f, false);
                    }
                } else if (strcmp(name, "ice_tile") == 0) {
                    /* Tinted floor overlay (lighter blue tint) */
                    add_box(mesh, fx + 0.02f, 0.001f, fz + 0.02f,
                            0.96f, 0.005f, 0.96f,
                            0.55f, 0.75f, 0.85f, 0.6f, false);
                } else if (strcmp(name, "crumbling_floor") == 0) {
                    /* Cracked floor tile with visible gap lines */
                    add_box(mesh, fx + 0.05f, 0.001f, fz + 0.05f,
                            0.42f, 0.008f, 0.42f,
                            0.45f, 0.40f, 0.30f, 1.0f, false);
                    add_box(mesh, fx + 0.53f, 0.001f, fz + 0.53f,
                            0.42f, 0.008f, 0.42f,
                            0.45f, 0.40f, 0.30f, 1.0f, false);
                    add_box(mesh, fx + 0.05f, 0.001f, fz + 0.53f,
                            0.42f, 0.006f, 0.42f,
                            0.42f, 0.38f, 0.28f, 1.0f, false);
                    add_box(mesh, fx + 0.53f, 0.001f, fz + 0.05f,
                            0.42f, 0.006f, 0.42f,
                            0.42f, 0.38f, 0.28f, 1.0f, false);
                } else {
                    /* Default: subtle floor accent marking */
                    float inset = 0.2f;
                    add_box(mesh, fx + inset, 0.001f, fz + inset,
                            1.0f - 2.0f * inset, 0.005f, 1.0f - 2.0f * inset,
                            biome->palette.accent[0] * 0.7f,
                            biome->palette.accent[1] * 0.7f,
                            biome->palette.accent[2] * 0.7f,
                            0.5f, false);
                }
            }
        }
    }
}

/* ---------- Step 8: Floor decorations ---------- */

static void gen_floor_deco(VoxelMesh* mesh, const Grid* grid,
                           const BiomeConfig* biome, RNG* rng) {
    if (biome->floor_decorations.prefab_count == 0 ||
        biome->floor_decorations.density <= 0.0f)
        return;

    for (int r = 0; r < grid->rows; r++) {
        for (int c = 0; c < grid->cols; c++) {
            const Cell* cell = grid_cell_const(grid, c, r);
            if (cell->impassable) continue;
            if (cell->feature_count > 0) continue; /* don't clutter feature tiles */

            int placed = 0;
            while (placed < biome->floor_decorations.max_per_tile) {
                if (rng_float(rng) > biome->floor_decorations.density) break;

                int pi = rng_int(rng, biome->floor_decorations.prefab_count);
                const char* pname = biome->floor_decorations.prefab_names[pi];
                const BiomePrefab* prefab = biome_config_find_prefab(biome, pname);
                if (!prefab) break;

                int rot = rng_int(rng, 4);
                float ox = (float)c + 0.2f + rng_float(rng) * 0.6f;
                float oz = (float)r + 0.2f + rng_float(rng) * 0.6f;

                for (int bi = 0; bi < prefab->box_count; bi++) {
                    const PrefabBox* pb = &prefab->boxes[bi];
                    float dx = pb->dx, dz = pb->dz;
                    float sx = pb->sx, sz = pb->sz;

                    for (int ri = 0; ri < rot; ri++) {
                        float tmp = dx; dx = -dz; dz = tmp;
                        tmp = sx; sx = sz; sz = tmp;
                    }

                    add_box(mesh,
                            ox + dx - sx * 0.5f,
                            pb->dy,
                            oz + dz - sz * 0.5f,
                            sx, pb->sy, sz,
                            pb->r, pb->g, pb->b, pb->a,
                            pb->no_cull);
                }

                placed++;
            }
        }
    }
}

/* ---------- Step 9: Wall decorations ---------- */

static void gen_wall_deco(VoxelMesh* mesh, const Grid* grid,
                          const BiomeConfig* biome, RNG* rng) {
    if (biome->wall_decorations.prefab_count == 0 ||
        biome->wall_decorations.density <= 0.0f)
        return;

    for (int r = 0; r < grid->rows; r++) {
        for (int c = 0; c < grid->cols; c++) {
            /* Decorate north walls */
            if (grid_has_wall(grid, c, r, DIR_NORTH) &&
                !is_door(grid, c, r, DIR_NORTH)) {
                if (rng_float(rng) < biome->wall_decorations.density) {
                    int pi = rng_int(rng, biome->wall_decorations.prefab_count);
                    const char* pname = biome->wall_decorations.prefab_names[pi];
                    const BiomePrefab* prefab = biome_config_find_prefab(biome, pname);
                    if (prefab) {
                        float ox = (float)c + 0.5f;
                        float oy = WALL_HEIGHT * 0.3f;
                        float oz = (float)(r + 1);
                        for (int bi = 0; bi < prefab->box_count; bi++) {
                            const PrefabBox* pb = &prefab->boxes[bi];
                            add_box(mesh,
                                    ox + pb->dx - pb->sx * 0.5f,
                                    oy + pb->dy,
                                    oz + pb->dz - pb->sz * 0.5f,
                                    pb->sx, pb->sy, pb->sz,
                                    pb->r, pb->g, pb->b, pb->a,
                                    pb->no_cull);
                        }
                    }
                }
            }

            /* Decorate east walls */
            if (grid_has_wall(grid, c, r, DIR_EAST) &&
                !is_door(grid, c, r, DIR_EAST)) {
                if (rng_float(rng) < biome->wall_decorations.density) {
                    int pi = rng_int(rng, biome->wall_decorations.prefab_count);
                    const char* pname = biome->wall_decorations.prefab_names[pi];
                    const BiomePrefab* prefab = biome_config_find_prefab(biome, pname);
                    if (prefab) {
                        float ox = (float)(c + 1);
                        float oy = WALL_HEIGHT * 0.3f;
                        float oz = (float)r + 0.5f;
                        for (int bi = 0; bi < prefab->box_count; bi++) {
                            const PrefabBox* pb = &prefab->boxes[bi];
                            add_box(mesh,
                                    ox + pb->dx - pb->sx * 0.5f,
                                    oy + pb->dy,
                                    oz + pb->dz - pb->sz * 0.5f,
                                    pb->sx, pb->sy, pb->sz,
                                    pb->r, pb->g, pb->b, pb->a,
                                    pb->no_cull);
                        }
                    }
                }
            }
        }
    }
}

/* ---------- Step 10: Lantern pillars ---------- */

static void gen_lanterns(VoxelMesh* mesh, const Grid* grid,
                         const BiomeConfig* biome, RNG* rng,
                         DioramaGenResult* result) {
    if (biome->lanterns.density <= 0.0f) return;

    float pillar_w = 0.08f;
    float pillar_h = WALL_HEIGHT * 1.2f;
    float glow_box = 0.06f;
    float glow_h = 0.08f;

    /* Place at wall corners and endpoints */
    for (int r = 0; r <= grid->rows; r++) {
        for (int c = 0; c <= grid->cols; c++) {
            /* Count walls meeting at this corner */
            int wall_count = 0;
            bool corner = false;

            if (r < grid->rows && c < grid->cols &&
                grid_has_wall(grid, c, r, DIR_NORTH)) wall_count++;
            if (r > 0 && c < grid->cols &&
                grid_has_wall(grid, c, r - 1, DIR_NORTH)) wall_count++;
            if (r < grid->rows && c < grid->cols &&
                grid_has_wall(grid, c, r, DIR_EAST)) wall_count++;
            if (r < grid->rows && c > 0 &&
                grid_has_wall(grid, c - 1, r, DIR_EAST)) wall_count++;

            if (biome->lanterns.place_at_corners && wall_count >= 2) corner = true;
            if (biome->lanterns.place_at_wall_ends && wall_count == 1) corner = true;

            if (!corner) continue;
            if (rng_float(rng) > biome->lanterns.density) continue;

            float px = (float)c - pillar_w * 0.5f;
            float pz = (float)r - pillar_w * 0.5f;

            /* Pillar */
            add_box(mesh, px, 0.0f, pz,
                    pillar_w, pillar_h, pillar_w,
                    biome->palette.wall[0] * 0.8f,
                    biome->palette.wall[1] * 0.8f,
                    biome->palette.wall[2] * 0.8f,
                    1.0f, true);

            /* Glow box on top */
            add_box(mesh,
                    (float)c - glow_box * 0.5f,
                    pillar_h,
                    (float)r - glow_box * 0.5f,
                    glow_box, glow_h, glow_box,
                    biome->lanterns.glow_color[0],
                    biome->lanterns.glow_color[1],
                    biome->lanterns.glow_color[2],
                    1.0f, true);

            /* Record point light */
            add_light(result,
                      (float)c, pillar_h + glow_h * 0.5f, (float)r,
                      biome->lanterns.glow_color[0] * 0.6f,
                      biome->lanterns.glow_color[1] * 0.6f,
                      biome->lanterns.glow_color[2] * 0.6f,
                      2.5f);
        }
    }
}

/* ---------- Step 11: Exit light ---------- */

static void gen_exit_light(VoxelMesh* mesh, const Grid* grid,
                           const BiomeConfig* biome,
                           DioramaGenResult* result) {
    int ec = grid->exit_col;
    int er = grid->exit_row;

    /* Amber beam boxes rising from exit tile */
    float beam_w = 0.06f;
    float beam_h = 0.4f;

    add_box(mesh,
            (float)ec + 0.5f - beam_w * 0.5f,
            0.02f,
            (float)er + 0.5f - beam_w * 0.5f,
            beam_w, beam_h, beam_w,
            biome->palette.accent[0],
            biome->palette.accent[1],
            biome->palette.accent[2],
            0.4f, true);

    /* Smaller secondary beams */
    for (int bi = 0; bi < 3; bi++) {
        float angle = (float)bi * (float)M_PI * 2.0f / 3.0f;
        float br = 0.15f;
        float bx = (float)ec + 0.5f + cosf(angle) * br - beam_w * 0.3f;
        float bz = (float)er + 0.5f + sinf(angle) * br - beam_w * 0.3f;

        add_box(mesh, bx, 0.02f, bz,
                beam_w * 0.6f, beam_h * 0.7f, beam_w * 0.6f,
                biome->palette.accent[0] * 0.9f,
                biome->palette.accent[1] * 0.9f,
                biome->palette.accent[2] * 0.7f,
                0.3f, true);
    }

    /* Warm point light at exit */
    add_light(result,
              (float)ec + 0.5f, beam_h * 0.5f, (float)er + 0.5f,
              biome->palette.accent[0] * 0.8f,
              biome->palette.accent[1] * 0.6f,
              biome->palette.accent[2] * 0.3f,
              3.0f);
}

/* ---------- Step 12: Edge border ---------- */

static void gen_edge_border(VoxelMesh* mesh, const Grid* grid,
                            const BiomeConfig* biome) {
    int depth = biome->edge_border.depth;
    float border_h = FLOOR_THICKNESS * 0.8f;
    float overhang = 0.5f;

    /* South edge */
    for (int i = 0; i < depth; i++) {
        float z = -(float)(i + 1) * 0.5f - overhang + (float)i * 0.5f;
        z = -overhang - (float)i * 0.5f;
        add_box_ao(mesh,
                   -overhang, -FLOOR_THICKNESS, z,
                   (float)grid->cols + 2.0f * overhang, border_h, 0.5f,
                   biome->palette.platform_side[0] * 0.9f,
                   biome->palette.platform_side[1] * 0.9f,
                   biome->palette.platform_side[2] * 0.9f,
                   1.0f, false, AO_MODE_NONE);
    }

    /* North edge */
    for (int i = 0; i < depth; i++) {
        float z = (float)grid->rows + overhang + (float)i * 0.5f;
        add_box_ao(mesh,
                   -overhang, -FLOOR_THICKNESS, z,
                   (float)grid->cols + 2.0f * overhang, border_h, 0.5f,
                   biome->palette.platform_side[0] * 0.9f,
                   biome->palette.platform_side[1] * 0.9f,
                   biome->palette.platform_side[2] * 0.9f,
                   1.0f, false, AO_MODE_NONE);
    }

    /* West edge */
    for (int i = 0; i < depth; i++) {
        float x = -overhang - (float)i * 0.5f;
        add_box_ao(mesh,
                   x - 0.5f, -FLOOR_THICKNESS, -overhang,
                   0.5f, border_h, (float)grid->rows + 2.0f * overhang,
                   biome->palette.platform_side[0] * 0.9f,
                   biome->palette.platform_side[1] * 0.9f,
                   biome->palette.platform_side[2] * 0.9f,
                   1.0f, false, AO_MODE_NONE);
    }

    /* East edge */
    for (int i = 0; i < depth; i++) {
        float x = (float)grid->cols + overhang + (float)i * 0.5f;
        add_box_ao(mesh,
                   x, -FLOOR_THICKNESS, -overhang,
                   0.5f, border_h, (float)grid->rows + 2.0f * overhang,
                   biome->palette.platform_side[0] * 0.9f,
                   biome->palette.platform_side[1] * 0.9f,
                   biome->palette.platform_side[2] * 0.9f,
                   1.0f, false, AO_MODE_NONE);
    }
}

/* ---------- Public API ---------- */

void diorama_generate(VoxelMesh* mesh, const Grid* grid,
                      const BiomeConfig* biome, DioramaGenResult* result) {
    memset(result, 0, sizeof(DioramaGenResult));

    RNG rng;
    rng_seed(&rng, grid->level_id);

    /* Pipeline (no monolithic platform — individual floor tiles only,
     * so below-surface effects like pits remain visible) */
    gen_floor(mesh, grid, biome, &rng);
    gen_walls(mesh, grid, biome, &rng);
    gen_doors(mesh, grid, biome);
    gen_impassable(mesh, grid, biome, &rng);
    gen_features(mesh, grid, biome);
    gen_floor_deco(mesh, grid, biome, &rng);
    gen_wall_deco(mesh, grid, biome, &rng);
    gen_lanterns(mesh, grid, biome, &rng, result);
    gen_exit_light(mesh, grid, biome, result);
    gen_edge_border(mesh, grid, biome);

    result->grid_cols = grid->cols;
    result->grid_rows = grid->rows;

    LOG_INFO("diorama_generate: %d boxes queued for biome '%s'",
             mesh->box_count, biome->id);
}
