#include "auto_turnstile.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int jc, jr;             /* junction column/row */
    bool clockwise;         /* true = CW, false = CCW */
} AutoTurnstileData;

/*
 * The 4 tiles around junction (jc, jr) are:
 *   NW = (jc-1, jr)     NE = (jc, jr)
 *   SW = (jc-1, jr-1)   SE = (jc, jr-1)
 *
 * CW rotation: NW→NE→SE→SW→NW
 * CCW rotation: NW→SW→SE→NE→NW
 *
 * Wall direction rotation CW: N→E, E→S, S→W, W→N
 * Wall direction rotation CCW: N→W, W→S, S→E, E→N
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

/* ── Vtable hooks ─────────────────────────────────────── */

static void at_on_environment_phase(Feature* self, Grid* grid) {
    AutoTurnstileData* d = (AutoTurnstileData*)self->data;

    /* Define the 4 tile positions: NW, NE, SE, SW */
    int tiles[4][2] = {
        { d->jc - 1, d->jr },      /* NW = index 0 */
        { d->jc,     d->jr },      /* NE = index 1 */
        { d->jc,     d->jr - 1 },  /* SE = index 2 */
        { d->jc - 1, d->jr - 1 },  /* SW = index 3 */
    };

    /* Validate all 4 tiles are in bounds */
    for (int i = 0; i < 4; i++) {
        if (!grid_in_bounds(grid, tiles[i][0], tiles[i][1])) return;
    }

    /* CW mapping:  0→1, 1→2, 2→3, 3→0
     * CCW mapping: 0→3, 3→2, 2→1, 1→0 */

    /* Save wall state for all 4 tiles */
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
        int dest_idx = d->clockwise ? (i + 1) % 4 : (i + 3) % 4;

        for (int dd = 0; dd < DIR_COUNT; dd++) {
            if (saved_walls[i][dd]) {
                Direction new_dir = d->clockwise
                    ? rotate_dir_cw((Direction)dd)
                    : rotate_dir_ccw((Direction)dd);
                grid_set_wall(grid, tiles[dest_idx][0], tiles[dest_idx][1],
                              new_dir, true);
            }
        }
    }

    /* Move actors on the 4 tiles */
    for (int i = 0; i < 4; i++) {
        int dest_idx = d->clockwise ? (i + 1) % 4 : (i + 3) % 4;

        if (grid->theseus_col == tiles[i][0] &&
            grid->theseus_row == tiles[i][1]) {
            grid->theseus_col = tiles[dest_idx][0];
            grid->theseus_row = tiles[dest_idx][1];
        }

        if (grid->minotaur_col == tiles[i][0] &&
            grid->minotaur_row == tiles[i][1]) {
            grid->minotaur_col = tiles[dest_idx][0];
            grid->minotaur_row = tiles[dest_idx][1];
        }
    }

    /* Move features on the 4 tiles (except this turnstile itself) */
    for (int fi = 0; fi < grid->feature_count; fi++) {
        Feature* feat = grid->features[fi];
        if (feat == self) continue;

        for (int i = 0; i < 4; i++) {
            if (feat->col == tiles[i][0] && feat->row == tiles[i][1]) {
                int dest_idx = d->clockwise ? (i + 1) % 4 : (i + 3) % 4;
                feat->col = tiles[dest_idx][0];
                feat->row = tiles[dest_idx][1];
                break;
            }
        }
    }

    /* Rebuild cell->feature links since features may have moved */
    grid_rebuild_feature_links(grid);
}

static size_t at_snapshot_size(const Feature* self) {
    (void)self;
    return 0;  /* no mutable internal state (walls/positions are snapshotted by undo) */
}

static void at_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable auto_turnstile_vt = {
    .name                  = "auto_turnstile",
    .blocks_movement       = NULL,
    .on_pre_move           = NULL,
    .on_enter              = NULL,
    .on_leave              = NULL,
    .on_push               = NULL,
    .on_environment_phase  = at_on_environment_phase,
    .is_hazardous          = NULL,
    .snapshot_size          = at_snapshot_size,
    .snapshot_save          = NULL,
    .snapshot_restore       = NULL,
    .destroy               = at_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* auto_turnstile_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&auto_turnstile_vt, col, row);
    if (!f) return NULL;

    AutoTurnstileData* d = calloc(1, sizeof(AutoTurnstileData));
    if (!d) { feature_free(f); return NULL; }

    d->jc = col;
    d->jr = row;
    d->clockwise = true;

    if (config) {
        const cJSON* jc_item = cJSON_GetObjectItemCaseSensitive(config, "junction_col");
        if (cJSON_IsNumber(jc_item)) d->jc = jc_item->valueint;

        const cJSON* jr_item = cJSON_GetObjectItemCaseSensitive(config, "junction_row");
        if (cJSON_IsNumber(jr_item)) d->jr = jr_item->valueint;

        const cJSON* dir_item = cJSON_GetObjectItemCaseSensitive(config, "direction");
        if (cJSON_IsString(dir_item)) {
            d->clockwise = (strcmp(dir_item->valuestring, "ccw") != 0);
        }
    }

    f->data = d;
    return f;
}
