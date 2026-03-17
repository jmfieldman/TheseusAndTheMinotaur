#include "tile_physics.h"
#include "grid.h"
#include "features/groove_box.h"
#include "features/auto_turnstile.h"
#include "features/conveyor.h"
#include "../data/biome_config.h"

#include <string.h>

/*
 * Rendering-layer height constants.  Duplicated from diorama_gen.c because
 * those are static #defines in a render file and we are in game/.  If these
 * drift, a static_assert or shared header can reconcile them later.
 */
#define CONVEYOR_HEIGHT     0.10f   /* conveyor / auto-turnstile platform elevation */
#define WALL_THICKNESS      0.20f   /* wall slab thickness (world units) */

/* ── Surface query ─────────────────────────────────────── */

TileSurface tile_physics_query(const Grid* grid,
                               const BiomeConfig* biome,
                               int col, int row) {
    TileSurface s = { .surface_y = 0.0f, .is_pit = false, .is_impassable = false };

    /* Out-of-bounds → bottomless void */
    if (!grid_in_bounds(grid, col, row)) {
        s.is_pit = true;
        return s;
    }

    const Cell* cell = grid_cell_const(grid, col, row);
    if (!cell) {
        s.is_pit = true;
        return s;
    }

    /* Impassable cell: distinguish solid block vs. deadly pit.
     * If any feature on this cell reports is_hazardous, it's a pit
     * (collapsed crumbling floor, moving-platform void).
     * Otherwise it's a solid block. */
    if (cell->impassable) {
        bool hazardous = false;
        for (int i = 0; i < cell->feature_count; i++) {
            const Feature* f = cell->features[i];
            if (f && f->vt && f->vt->is_hazardous &&
                f->vt->is_hazardous(f, grid, col, row)) {
                hazardous = true;
                break;
            }
        }
        if (hazardous) {
            s.is_pit = true;
        } else {
            s.is_impassable = true;
        }
        return s;
    }

    /* Check features for surface height modifications */
    for (int i = 0; i < cell->feature_count; i++) {
        const Feature* f = cell->features[i];
        if (!f || !f->vt || !f->vt->name) continue;

        /* Conveyor belt: elevated platform */
        if (strcmp(f->vt->name, "conveyor") == 0) {
            s.surface_y = CONVEYOR_HEIGHT;
            return s;
        }

        /* Auto-turnstile: elevated platform (same height as conveyors) */
        if (strcmp(f->vt->name, "auto_turnstile") == 0) {
            /* The turnstile feature lives on one tile but covers a 2×2 area.
             * Check if (col, row) is one of the 4 junction tiles. */
            int jc, jr;
            bool cw;
            if (auto_turnstile_get_junction(f, &jc, &jr, &cw)) {
                int cells[4][2] = {
                    { jc - 1, jr     },
                    { jc,     jr     },
                    { jc,     jr - 1 },
                    { jc - 1, jr - 1 }
                };
                for (int t = 0; t < 4; t++) {
                    if (cells[t][0] == col && cells[t][1] == row) {
                        s.surface_y = CONVEYOR_HEIGHT;
                        return s;
                    }
                }
            }
        }
    }

    /* Groove trench: check if this tile is on a groove path.
     * Groove paths are defined by groove_box features — iterate all grid
     * features and check their paths. */
    for (int fi = 0; fi < grid->feature_count; fi++) {
        const Feature* f = grid->features[fi];
        if (!f) continue;

        int path_cols[32], path_rows[32];
        int path_len = groove_box_get_path(f, path_cols, path_rows, 32);
        for (int pi = 0; pi < path_len; pi++) {
            if (path_cols[pi] == col && path_rows[pi] == row) {
                s.surface_y = -biome->groove_trench.trench_depth;
                return s;
            }
        }
    }

    return s;
}

/* ── Wall query ────────────────────────────────────────── */

bool tile_physics_has_wall(const Grid* grid,
                           int col, int row, Direction side) {
    /* Actual grid wall */
    if (grid_has_wall(grid, col, row, side)) return true;

    /* Impassable neighbor acts as a wall boundary.
     * An impassable tile that is a pit doesn't act as a wall — voxels
     * should be able to fly over pits.  Only solid blocks do. */
    int nc = col + direction_dcol(side);
    int nr = row + direction_drow(side);

    if (!grid_in_bounds(grid, nc, nr)) return false; /* OOB is pit, not wall */

    const Cell* neighbor = grid_cell_const(grid, nc, nr);
    if (neighbor && neighbor->impassable) {
        /* Check if this impassable cell is a solid block (not a pit).
         * A pit has a hazardous feature — skip those. */
        bool hazardous = false;
        for (int i = 0; i < neighbor->feature_count; i++) {
            const Feature* f = neighbor->features[i];
            if (f && f->vt && f->vt->is_hazardous &&
                f->vt->is_hazardous(f, grid, nc, nr)) {
                hazardous = true;
                break;
            }
        }
        if (!hazardous) return true;  /* solid block */
    }

    return false;
}

/* ── Wall coordinate ───────────────────────────────────── */

float tile_physics_wall_coord(const Grid* grid,
                              int col, int row, Direction side) {
    (void)grid;

    /* Each tile spans [col, col+1] in X and [row, row+1] in Z.
     * Walls have WALL_THICKNESS centered on the tile edge.
     * Return the inner face (the face a voxel bounces off). */
    float half_wall = WALL_THICKNESS * 0.5f;

    switch (side) {
        case DIR_NORTH: return (float)(row + 1) - half_wall; /* Z coord */
        case DIR_SOUTH: return (float)row       + half_wall; /* Z coord */
        case DIR_EAST:  return (float)(col + 1) - half_wall; /* X coord */
        case DIR_WEST:  return (float)col       + half_wall; /* X coord */
        default:        return 0.0f;
    }
}
