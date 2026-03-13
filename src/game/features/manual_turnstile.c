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
 * Same 4-tile layout as auto_turnstile:
 *   NW = (jc-1, jr)     NE = (jc, jr)
 *   SW = (jc-1, jr-1)   SE = (jc, jr-1)
 *
 * When Theseus pushes, we determine the rotation direction from the
 * push direction and rotate only walls (not actors or features).
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
        { jc - 1, jr },      /* NW */
        { jc,     jr },      /* NE */
        { jc,     jr - 1 },  /* SE */
        { jc - 1, jr - 1 },  /* SW */
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

/* ── Vtable hooks ─────────────────────────────────────── */

static bool mt_on_push(Feature* self, Grid* grid,
                        int from_col, int from_row, Direction dir) {
    ManualTurnstileData* d = (ManualTurnstileData*)self->data;

    /* Check if Theseus is on one of the 4 junction tiles and pushing
     * against a wall that touches the junction */
    int tiles[4][2] = {
        { d->jc - 1, d->jr },
        { d->jc,     d->jr },
        { d->jc,     d->jr - 1 },
        { d->jc - 1, d->jr - 1 },
    };

    bool on_junction_tile = false;
    for (int i = 0; i < 4; i++) {
        if (from_col == tiles[i][0] && from_row == tiles[i][1]) {
            on_junction_tile = true;
            break;
        }
    }

    if (!on_junction_tile) return false;

    /* Check if there's actually a wall in the push direction at this tile */
    if (!grid_has_wall(grid, from_col, from_row, dir)) return false;

    /* Determine rotation direction based on push direction.
     * Pushing east or north → CW. Pushing west or south → CCW. */
    bool clockwise = (dir == DIR_EAST || dir == DIR_NORTH);

    rotate_junction_walls(grid, d->jc, d->jr, clockwise);

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
