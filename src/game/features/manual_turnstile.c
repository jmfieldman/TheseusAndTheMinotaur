#include "manual_turnstile.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int jc, jr;             /* junction column/row */
} ManualTurnstileData;

/*
 * The 4 tiles around junction (jc, jr) are:
 *   NW = (jc-1, jr)     NE = (jc, jr)       index 0, 1
 *   SW = (jc-1, jr-1)   SE = (jc, jr-1)     index 3, 2
 *
 * CW cycle:  NW(0) → NE(1) → SE(2) → SW(3) → NW(0)
 * CCW cycle: NW(0) → SW(3) → SE(2) → NE(1) → NW(0)
 *
 * When Theseus pushes from a junction tile, the push direction determines
 * which adjacent tile he's pushing toward.  If that neighbor is the CW-next
 * tile in the cycle, we rotate CW; if it's the CCW-next, we rotate CCW.
 * Theseus moves to the destination tile along with the walls.
 *
 * Valid pushes per tile (only junction-facing directions):
 *   NW(0): East→NE(1) = CW,   South→SW(3) = CCW
 *   NE(1): South→SE(2) = CW,  West→NW(0)  = CCW
 *   SE(2): West→SW(3) = CW,   North→NE(1) = CCW
 *   SW(3): North→NW(0) = CW,  East→SE(2)  = CCW
 */

static Direction rotate_dir_cw(Direction d) {
    switch (d) {
        case DIR_NORTH: return DIR_EAST;
        case DIR_EAST:  return DIR_SOUTH;
        case DIR_SOUTH: return DIR_WEST;
        case DIR_WEST:  return DIR_NORTH;
        default: return d;
    }
}

static Direction rotate_dir_ccw(Direction d) {
    switch (d) {
        case DIR_NORTH: return DIR_WEST;
        case DIR_WEST:  return DIR_SOUTH;
        case DIR_SOUTH: return DIR_EAST;
        case DIR_EAST:  return DIR_NORTH;
        default: return d;
    }
}

static void rotate_junction_walls(Grid* grid, int jc, int jr, bool clockwise) {
    int tiles[4][2] = {
        { jc - 1, jr },      /* NW = 0 */
        { jc,     jr },      /* NE = 1 */
        { jc,     jr - 1 },  /* SE = 2 */
        { jc - 1, jr - 1 },  /* SW = 3 */
    };

    for (int i = 0; i < 4; i++) {
        if (!grid_in_bounds(grid, tiles[i][0], tiles[i][1])) return;
    }

    bool saved_walls[4][DIR_COUNT];
    for (int i = 0; i < 4; i++) {
        const Cell* c = grid_cell_const(grid, tiles[i][0], tiles[i][1]);
        for (int dd = 0; dd < DIR_COUNT; dd++) {
            saved_walls[i][dd] = c->walls[dd];
        }
    }

    /* Clear all walls on the 4 tiles */
    for (int i = 0; i < 4; i++) {
        for (int dd = 0; dd < DIR_COUNT; dd++) {
            grid_set_wall(grid, tiles[i][0], tiles[i][1], (Direction)dd, false);
        }
    }

    /* Apply rotated walls */
    for (int i = 0; i < 4; i++) {
        int dest_idx = clockwise ? (i + 1) % 4 : (i + 3) % 4;
        for (int dd = 0; dd < DIR_COUNT; dd++) {
            if (saved_walls[i][dd]) {
                Direction new_dir = clockwise
                    ? rotate_dir_cw((Direction)dd)
                    : rotate_dir_ccw((Direction)dd);
                grid_set_wall(grid, tiles[dest_idx][0], tiles[dest_idx][1],
                              new_dir, true);
            }
        }
    }
}

/*
 * Given a tile index (0-3) and a push direction, determine:
 *   - dest_idx: the tile index Theseus would move to
 *   - clockwise: whether the rotation is CW or CCW
 * Returns true if this is a valid junction push, false otherwise.
 */
static bool resolve_push(int tile_idx, Direction dir,
                          int* out_dest_idx, bool* out_clockwise) {
    /*
     * For each tile, there are exactly 2 valid push directions
     * (the ones facing the junction center).
     *
     * Table of (tile_idx, push_dir) → (dest_idx, cw?):
     *   0,E → 1,CW    0,S → 3,CCW
     *   1,S → 2,CW    1,W → 0,CCW
     *   2,W → 3,CW    2,N → 1,CCW
     *   3,N → 0,CW    3,E → 2,CCW
     */
    static const struct {
        Direction cw_dir;   /* push direction that produces CW rotation */
        int cw_dest;
        Direction ccw_dir;  /* push direction that produces CCW rotation */
        int ccw_dest;
    } table[4] = {
        /* NW(0) */ { DIR_EAST,  1, DIR_SOUTH, 3 },
        /* NE(1) */ { DIR_SOUTH, 2, DIR_WEST,  0 },
        /* SE(2) */ { DIR_WEST,  3, DIR_NORTH, 1 },
        /* SW(3) */ { DIR_NORTH, 0, DIR_EAST,  2 },
    };

    if (tile_idx < 0 || tile_idx > 3) return false;

    if (dir == table[tile_idx].cw_dir) {
        *out_dest_idx = table[tile_idx].cw_dest;
        *out_clockwise = true;
        return true;
    }
    if (dir == table[tile_idx].ccw_dir) {
        *out_dest_idx = table[tile_idx].ccw_dest;
        *out_clockwise = false;
        return true;
    }

    return false;  /* push direction doesn't face the junction */
}

/* ── Vtable hooks ─────────────────────────────────────── */

static bool mt_on_push(Feature* self, Grid* grid,
                        int from_col, int from_row, Direction dir) {
    ManualTurnstileData* d = (ManualTurnstileData*)self->data;

    /* Identify the 4 junction tiles */
    int tiles[4][2] = {
        { d->jc - 1, d->jr },      /* NW = 0 */
        { d->jc,     d->jr },      /* NE = 1 */
        { d->jc,     d->jr - 1 },  /* SE = 2 */
        { d->jc - 1, d->jr - 1 },  /* SW = 3 */
    };

    /* Find which tile Theseus is on */
    int tile_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (from_col == tiles[i][0] && from_row == tiles[i][1]) {
            tile_idx = i;
            break;
        }
    }
    if (tile_idx < 0) return false;

    /* Check if there's actually a wall in the push direction at this tile */
    if (!grid_has_wall(grid, from_col, from_row, dir)) return false;

    /* Determine rotation direction and destination tile */
    int dest_idx;
    bool clockwise;
    if (!resolve_push(tile_idx, dir, &dest_idx, &clockwise)) return false;

    /* Rotate all junction walls */
    rotate_junction_walls(grid, d->jc, d->jr, clockwise);

    /* Move Theseus to the destination tile (he pushes along with the wall) */
    grid->theseus_col = tiles[dest_idx][0];
    grid->theseus_row = tiles[dest_idx][1];

    LOG_INFO("manual_turnstile: push from (%d,%d) dir=%d → %s, Theseus to (%d,%d)",
             from_col, from_row, dir, clockwise ? "CW" : "CCW",
             tiles[dest_idx][0], tiles[dest_idx][1]);

    return true;  /* action consumed */
}

static size_t mt_snapshot_size(const Feature* self) {
    (void)self;
    return 0;  /* no mutable internal state — walls snapshotted by undo */
}

static void mt_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable manual_turnstile_vt = {
    .name                  = "manual_turnstile",
    .blocks_movement       = NULL,
    .on_pre_move           = NULL,
    .on_enter              = NULL,
    .on_leave              = NULL,
    .on_push               = mt_on_push,
    .on_environment_phase  = NULL,
    .is_hazardous          = NULL,
    .snapshot_size          = mt_snapshot_size,
    .snapshot_save          = NULL,
    .snapshot_restore       = NULL,
    .destroy               = mt_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* manual_turnstile_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&manual_turnstile_vt, col, row);
    if (!f) return NULL;

    ManualTurnstileData* d = calloc(1, sizeof(ManualTurnstileData));
    if (!d) { feature_free(f); return NULL; }

    d->jc = col;
    d->jr = row;

    if (config) {
        const cJSON* jc_item = cJSON_GetObjectItemCaseSensitive(config, "junction_col");
        if (cJSON_IsNumber(jc_item)) d->jc = jc_item->valueint;

        const cJSON* jr_item = cJSON_GetObjectItemCaseSensitive(config, "junction_row");
        if (cJSON_IsNumber(jr_item)) d->jr = jr_item->valueint;
    }

    f->data = d;
    return f;
}
