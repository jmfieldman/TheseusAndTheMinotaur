#include "crumbling_floor.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    CF_INTACT,      /* passable, not yet stepped on */
    CF_STEPPED,     /* Theseus has been here — will collapse when he leaves */
    CF_COLLAPSED    /* deadly pit — impassable and hazardous */
} CrumblingState;

typedef struct {
    CrumblingState state;
} CrumblingFloorData;

/* ── Vtable hooks ─────────────────────────────────────── */

static bool cf_blocks_movement(const Feature* self, const Grid* grid,
                                EntityID who,
                                int from_col, int from_row,
                                int to_col, int to_row) {
    (void)grid; (void)from_col; (void)from_row;
    const CrumblingFloorData* d = (const CrumblingFloorData*)self->data;

    /* Collapsed pit blocks Minotaur (and Theseus — is_hazardous handles death) */
    if (d->state == CF_COLLAPSED &&
        to_col == self->col && to_row == self->row) {
        if (who == ENTITY_MINOTAUR) return true;
        /* For Theseus, let them enter so is_hazardous kills them */
    }

    return false;
}

static void cf_on_enter(Feature* self, Grid* grid, EntityID who,
                         int col, int row) {
    (void)grid; (void)col; (void)row;
    CrumblingFloorData* d = (CrumblingFloorData*)self->data;

    /* Mark that Theseus has stepped here; it will crumble when he leaves */
    if (who == ENTITY_THESEUS && d->state == CF_INTACT) {
        d->state = CF_STEPPED;
    }
}

static void cf_on_leave(Feature* self, Grid* grid, EntityID who,
                          int col, int row) {
    (void)col; (void)row;
    CrumblingFloorData* d = (CrumblingFloorData*)self->data;

    /* Theseus left a stepped tile — collapse it now */
    if (who == ENTITY_THESEUS && d->state == CF_STEPPED) {
        d->state = CF_COLLAPSED;
        Cell* cell = grid_cell(grid, self->col, self->row);
        if (cell) cell->impassable = true;
    }
}

static bool cf_is_hazardous(const Feature* self, const Grid* grid,
                              int col, int row) {
    (void)grid; (void)col; (void)row;
    const CrumblingFloorData* d = (const CrumblingFloorData*)self->data;
    return d->state == CF_COLLAPSED;
}

static size_t cf_snapshot_size(const Feature* self) {
    (void)self;
    return sizeof(CrumblingFloorData);
}

static void cf_snapshot_save(const Feature* self, void* buf) {
    memcpy(buf, self->data, sizeof(CrumblingFloorData));
}

static void cf_snapshot_restore(Feature* self, const void* buf) {
    memcpy(self->data, buf, sizeof(CrumblingFloorData));
    /* Cell impassable state restored by undo cell snapshot */
}

static void cf_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable crumbling_floor_vt = {
    .name                  = "crumbling_floor",
    .blocks_movement       = cf_blocks_movement,
    .on_pre_move           = NULL,
    .on_enter              = cf_on_enter,
    .on_leave              = cf_on_leave,
    .on_push               = NULL,
    .on_environment_phase  = NULL,
    .is_hazardous          = cf_is_hazardous,
    .snapshot_size          = cf_snapshot_size,
    .snapshot_save          = cf_snapshot_save,
    .snapshot_restore       = cf_snapshot_restore,
    .destroy               = cf_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* crumbling_floor_create(int col, int row, const cJSON* config) {
    (void)config;
    Feature* f = feature_create(&crumbling_floor_vt, col, row);
    if (!f) return NULL;

    CrumblingFloorData* d = calloc(1, sizeof(CrumblingFloorData));
    if (!d) { feature_free(f); return NULL; }

    d->state = CF_INTACT;

    f->data = d;
    return f;
}
