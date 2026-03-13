#include "crumbling_floor.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    CF_INTACT,      /* passable, not yet triggered */
    CF_MARKED,      /* actor left or Theseus waited — will collapse next env phase */
    CF_COLLAPSED    /* deadly pit — impassable and hazardous */
} CrumblingState;

typedef struct {
    CrumblingState state;
    bool theseus_present;   /* track for the "wait" collapse case */
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
    if (who == ENTITY_THESEUS) {
        d->theseus_present = true;
    }
}

static void cf_on_leave(Feature* self, Grid* grid, EntityID who,
                          int col, int row) {
    (void)grid; (void)col; (void)row;
    CrumblingFloorData* d = (CrumblingFloorData*)self->data;

    if (who == ENTITY_THESEUS) {
        d->theseus_present = false;
        if (d->state == CF_INTACT) {
            d->state = CF_MARKED;
        }
    }
}

static void cf_on_environment_phase(Feature* self, Grid* grid) {
    CrumblingFloorData* d = (CrumblingFloorData*)self->data;

    if (d->state == CF_INTACT && d->theseus_present) {
        /* Theseus waited on the tile — collapse immediately */
        d->state = CF_COLLAPSED;
        Cell* cell = grid_cell(grid, self->col, self->row);
        if (cell) cell->impassable = true;
    } else if (d->state == CF_MARKED) {
        /* Actor left — collapse */
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
    .on_environment_phase  = cf_on_environment_phase,
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
    d->theseus_present = false;

    f->data = d;
    return f;
}
