#include "moving_platform.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 32

typedef enum {
    MP_PINGPONG,
    MP_LOOP
} MPMode;

typedef struct {
    int path_cols[MAX_PATH_LENGTH];
    int path_rows[MAX_PATH_LENGTH];
    int path_length;
    int current_index;
    int direction;          /* +1 or -1 for pingpong */
    MPMode mode;
} MovingPlatformData;

/* ── Helpers ──────────────────────────────────────────── */

static void make_current_passable(const MovingPlatformData* d, Grid* grid) {
    Cell* cell = grid_cell(grid, d->path_cols[d->current_index],
                                  d->path_rows[d->current_index]);
    if (cell) cell->impassable = false;
}

static void make_current_impassable(const MovingPlatformData* d, Grid* grid) {
    Cell* cell = grid_cell(grid, d->path_cols[d->current_index],
                                  d->path_rows[d->current_index]);
    if (cell) cell->impassable = true;
}

/* ── Vtable hooks ─────────────────────────────────────── */

static bool mp_blocks_movement(const Feature* self, const Grid* grid,
                                EntityID who,
                                int from_col, int from_row,
                                int to_col, int to_row) {
    (void)grid; (void)from_col; (void)from_row;
    const MovingPlatformData* d = (const MovingPlatformData*)self->data;

    int plat_col = d->path_cols[d->current_index];
    int plat_row = d->path_rows[d->current_index];

    /* If trying to enter a path tile that is NOT the current platform position,
     * it's a pit — block Minotaur */
    if (to_col == plat_col && to_row == plat_row) {
        return false;  /* platform is here — passable */
    }

    /* Check if target is any tile on the path (a pit tile) */
    for (int i = 0; i < d->path_length; i++) {
        if (to_col == d->path_cols[i] && to_row == d->path_rows[i]) {
            if (who == ENTITY_MINOTAUR) return true;  /* pit blocks Minotaur */
            /* For Theseus, the pit is deadly — let them enter and die via is_hazardous */
        }
    }

    return false;
}

static bool mp_is_hazardous(const Feature* self, const Grid* grid,
                             int col, int row) {
    (void)grid;
    const MovingPlatformData* d = (const MovingPlatformData*)self->data;

    int plat_col = d->path_cols[d->current_index];
    int plat_row = d->path_rows[d->current_index];

    /* Hazardous if Theseus is on a path tile that is NOT the platform */
    if (col == plat_col && row == plat_row) return false;

    for (int i = 0; i < d->path_length; i++) {
        if (col == d->path_cols[i] && row == d->path_rows[i]) {
            return true;  /* on pit, no platform */
        }
    }

    return false;
}

static void mp_on_environment_phase(Feature* self, Grid* grid) {
    MovingPlatformData* d = (MovingPlatformData*)self->data;
    if (d->path_length < 2) return;

    int old_col = d->path_cols[d->current_index];
    int old_row = d->path_rows[d->current_index];

    /* Make old position impassable again (it's a pit) */
    make_current_impassable(d, grid);

    /* Advance along path */
    int next_index = d->current_index + d->direction;

    if (d->mode == MP_PINGPONG) {
        if (next_index >= d->path_length) {
            d->direction = -1;
            next_index = d->current_index + d->direction;
        } else if (next_index < 0) {
            d->direction = 1;
            next_index = d->current_index + d->direction;
        }
    } else {
        /* Loop */
        next_index = (next_index + d->path_length) % d->path_length;
    }

    d->current_index = next_index;

    int new_col = d->path_cols[d->current_index];
    int new_row = d->path_rows[d->current_index];

    /* Make new position passable (platform is here) */
    make_current_passable(d, grid);

    /* Update feature position */
    self->col = new_col;
    self->row = new_row;

    /* Move any actor that was on the old platform position to the new one */
    if (grid->theseus_col == old_col && grid->theseus_row == old_row) {
        grid->theseus_col = new_col;
        grid->theseus_row = new_row;
    }
    if (grid->minotaur_col == old_col && grid->minotaur_row == old_row) {
        grid->minotaur_col = new_col;
        grid->minotaur_row = new_row;
    }

    /* Rebuild feature links since platform moved */
    grid_rebuild_feature_links(grid);
}

static size_t mp_snapshot_size(const Feature* self) {
    (void)self;
    return sizeof(int) * 2;  /* current_index + direction */
}

static void mp_snapshot_save(const Feature* self, void* buf) {
    const MovingPlatformData* d = (const MovingPlatformData*)self->data;
    int* p = (int*)buf;
    p[0] = d->current_index;
    p[1] = d->direction;
}

static void mp_snapshot_restore(Feature* self, const void* buf) {
    MovingPlatformData* d = (MovingPlatformData*)self->data;
    const int* p = (const int*)buf;
    d->current_index = p[0];
    d->direction = p[1];
    /* Cell impassable state and feature position restored by undo */
}

static void mp_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable moving_platform_vt = {
    .name                  = "moving_platform",
    .blocks_movement       = mp_blocks_movement,
    .on_pre_move           = NULL,
    .on_enter              = NULL,
    .on_leave              = NULL,
    .on_push               = NULL,
    .on_environment_phase  = mp_on_environment_phase,
    .is_hazardous          = mp_is_hazardous,
    .snapshot_size          = mp_snapshot_size,
    .snapshot_save          = mp_snapshot_save,
    .snapshot_restore       = mp_snapshot_restore,
    .destroy               = mp_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* moving_platform_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&moving_platform_vt, col, row);
    if (!f) return NULL;

    MovingPlatformData* d = calloc(1, sizeof(MovingPlatformData));
    if (!d) { feature_free(f); return NULL; }

    d->current_index = 0;
    d->direction = 1;
    d->mode = MP_PINGPONG;

    if (config) {
        const cJSON* path_arr = cJSON_GetObjectItemCaseSensitive(config, "path");
        if (cJSON_IsArray(path_arr)) {
            const cJSON* item = NULL;
            int idx = 0;
            cJSON_ArrayForEach(item, path_arr) {
                if (idx >= MAX_PATH_LENGTH) break;
                const cJSON* c_item = cJSON_GetObjectItemCaseSensitive(item, "col");
                const cJSON* r_item = cJSON_GetObjectItemCaseSensitive(item, "row");
                if (cJSON_IsNumber(c_item) && cJSON_IsNumber(r_item)) {
                    d->path_cols[idx] = c_item->valueint;
                    d->path_rows[idx] = r_item->valueint;
                    idx++;
                }
            }
            d->path_length = idx;
        }

        const cJSON* mode_item = cJSON_GetObjectItemCaseSensitive(config, "mode");
        if (cJSON_IsString(mode_item)) {
            d->mode = (strcmp(mode_item->valuestring, "loop") == 0)
                      ? MP_LOOP : MP_PINGPONG;
        }

        const cJSON* idx_item = cJSON_GetObjectItemCaseSensitive(config, "initial_index");
        if (cJSON_IsNumber(idx_item)) {
            d->current_index = idx_item->valueint;
            if (d->current_index < 0 || d->current_index >= d->path_length) {
                d->current_index = 0;
            }
        }
    }

    /* Set feature position to current path position */
    if (d->path_length > 0) {
        f->col = d->path_cols[d->current_index];
        f->row = d->path_rows[d->current_index];
    }

    f->data = d;
    return f;
}
