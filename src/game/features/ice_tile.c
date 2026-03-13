#include "ice_tile.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

/*
 * Ice tile has no mutable data — the sliding logic is handled by
 * turn.c's ice_slide() function.  The ice_tile feature just signals
 * PREMOVE_SLIDE via on_pre_move so turn.c knows to engage sliding.
 */

/* ── Vtable hooks ─────────────────────────────────────── */

static PreMoveResult ice_on_pre_move(const Feature* self, const Grid* grid,
                                      int from_col, int from_row,
                                      int to_col, int to_row,
                                      Direction dir) {
    (void)grid; (void)from_col; (void)from_row; (void)dir;

    /* Signal slide only when Theseus is moving ONTO this ice tile */
    if (to_col == self->col && to_row == self->row) {
        return PREMOVE_SLIDE;
    }

    return PREMOVE_OK;
}

static const FeatureVTable ice_tile_vt = {
    .name                  = "ice_tile",
    .blocks_movement       = NULL,
    .on_pre_move           = ice_on_pre_move,
    .on_enter              = NULL,
    .on_leave              = NULL,
    .on_push               = NULL,
    .on_environment_phase  = NULL,
    .is_hazardous          = NULL,
    .snapshot_size          = NULL,
    .snapshot_save          = NULL,
    .snapshot_restore       = NULL,
    .destroy               = NULL,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* ice_tile_create(int col, int row, const cJSON* config) {
    (void)config;
    Feature* f = feature_create(&ice_tile_vt, col, row);
    /* No per-instance data needed */
    return f;
}
