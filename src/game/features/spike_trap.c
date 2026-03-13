#include "spike_trap.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool active;            /* true = spikes up (deadly, blocks minotaur) */
    bool armed;             /* true = will activate next environment phase */
    bool theseus_present;   /* true = Theseus is currently on this tile */
} SpikeTrapData;

/* ── Vtable hooks ──────────────────────────────────────── */

static bool spike_blocks_movement(const Feature* self, const Grid* grid,
                                   EntityID who,
                                   int from_col, int from_row,
                                   int to_col,   int to_row) {
    (void)grid; (void)from_col; (void)from_row;

    /* Only block the Minotaur when spikes are up */
    if (who != ENTITY_MINOTAUR) return false;

    const SpikeTrapData* d = (const SpikeTrapData*)self->data;
    if (!d->active) return false;

    /* Block if the minotaur is trying to enter this tile */
    return (to_col == self->col && to_row == self->row);
}

static void spike_on_enter(Feature* self, Grid* grid, EntityID who,
                            int col, int row) {
    (void)grid; (void)col; (void)row;
    SpikeTrapData* d = (SpikeTrapData*)self->data;

    if (who == ENTITY_THESEUS) {
        d->theseus_present = true;
    }
    /* Minotaur entering does NOT arm the trap */
}

static void spike_on_leave(Feature* self, Grid* grid, EntityID who,
                            int col, int row) {
    (void)grid; (void)col; (void)row;
    SpikeTrapData* d = (SpikeTrapData*)self->data;

    if (who == ENTITY_THESEUS) {
        d->theseus_present = false;
        /* Theseus leaving arms the trap (spikes will go up next env phase) */
        if (!d->active) {
            d->armed = true;
        }
    }
}

static void spike_on_environment_phase(Feature* self, Grid* grid) {
    (void)grid;
    SpikeTrapData* d = (SpikeTrapData*)self->data;

    if (d->active) {
        /* Spikes were up — retract after one turn */
        d->active = false;
    } else if (d->armed) {
        /* Armed — spikes shoot up */
        d->active = true;
        d->armed = false;
    } else if (d->theseus_present) {
        /* Theseus waited on the tile (didn't leave) — arm and fire */
        d->active = true;
    }
}

static bool spike_is_hazardous(const Feature* self, const Grid* grid,
                                int col, int row) {
    (void)grid; (void)col; (void)row;
    const SpikeTrapData* d = (const SpikeTrapData*)self->data;
    return d->active;
}

static size_t spike_snapshot_size(const Feature* self) {
    (void)self;
    return sizeof(SpikeTrapData);
}

static void spike_snapshot_save(const Feature* self, void* buf) {
    memcpy(buf, self->data, sizeof(SpikeTrapData));
}

static void spike_snapshot_restore(Feature* self, const void* buf) {
    memcpy(self->data, buf, sizeof(SpikeTrapData));
}

static void spike_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable spike_trap_vt = {
    .name                  = "spike_trap",
    .blocks_movement       = spike_blocks_movement,
    .on_pre_move           = NULL,
    .on_enter              = spike_on_enter,
    .on_leave              = spike_on_leave,
    .on_push               = NULL,
    .on_environment_phase  = spike_on_environment_phase,
    .is_hazardous          = spike_is_hazardous,
    .snapshot_size          = spike_snapshot_size,
    .snapshot_save          = spike_snapshot_save,
    .snapshot_restore       = spike_snapshot_restore,
    .destroy               = spike_destroy,
};

/* ── Factory ───────────────────────────────────────────── */

Feature* spike_trap_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&spike_trap_vt, col, row);
    if (!f) return NULL;

    SpikeTrapData* d = calloc(1, sizeof(SpikeTrapData));
    if (!d) {
        feature_free(f);
        return NULL;
    }

    /* Defaults: spikes down, not armed */
    d->active          = false;
    d->armed           = false;
    d->theseus_present = false;

    /* Parse config */
    if (config) {
        const cJSON* active_item = cJSON_GetObjectItemCaseSensitive(config, "initial_active");
        if (cJSON_IsBool(active_item)) {
            d->active = cJSON_IsTrue(active_item);
        }
    }

    f->data = d;
    return f;
}
