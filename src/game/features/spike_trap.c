#include "spike_trap.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int  up_turns;          /* config: how many env phases spikes stay up */
    int  active_remaining;  /* countdown while spikes are up; 0 = retracted */
    int  arm_turn;          /* turn_count when armed; -1 = not armed */
} SpikeTrapData;

/*
 * Spike trap lifecycle:
 *
 * 1. RETRACTED (active_remaining == 0, arm_turn == -1)
 *    - Safe to walk on.  Theseus stepping on arms the trap.
 *
 * 2. ARMED (active_remaining == 0, arm_turn >= 0)
 *    - Still safe this turn.  On the NEXT turn's environment phase the
 *      spikes fire regardless of who is standing there.
 *
 * 3. ACTIVE (active_remaining > 0)
 *    - Deadly (is_hazardous) and blocks the Minotaur.
 *    - Each environment phase decrements active_remaining.
 *    - When it reaches 0 the spikes retract → RETRACTED.
 */

/* ── Vtable hooks ──────────────────────────────────────── */

static bool spike_blocks_movement(const Feature* self, const Grid* grid,
                                   EntityID who,
                                   int from_col, int from_row,
                                   int to_col,   int to_row) {
    (void)grid; (void)from_col; (void)from_row;

    /* Only block the Minotaur when spikes are up */
    if (who != ENTITY_MINOTAUR) return false;

    const SpikeTrapData* d = (const SpikeTrapData*)self->data;
    if (d->active_remaining <= 0) return false;

    return (to_col == self->col && to_row == self->row);
}

static void spike_on_enter(Feature* self, Grid* grid, EntityID who,
                            int col, int row) {
    (void)col; (void)row;
    SpikeTrapData* d = (SpikeTrapData*)self->data;

    if (who != ENTITY_THESEUS) return;

    /* Arm the trap if it's currently retracted and not already armed */
    if (d->active_remaining <= 0 && d->arm_turn < 0) {
        d->arm_turn = grid->turn_count;
        LOG_INFO("spike_trap(%d,%d): armed on turn %d",
                 self->col, self->row, grid->turn_count);
    }
}

static void spike_on_environment_phase(Feature* self, Grid* grid) {
    SpikeTrapData* d = (SpikeTrapData*)self->data;

    if (d->active_remaining > 0) {
        /* Spikes are up — count down */
        d->active_remaining--;
        if (d->active_remaining <= 0) {
            LOG_INFO("spike_trap(%d,%d): retracted", self->col, self->row);
        }
    } else if (d->arm_turn >= 0 && grid->turn_count > d->arm_turn) {
        /* Armed on a previous turn — fire! */
        d->active_remaining = d->up_turns;
        d->arm_turn = -1;
        LOG_INFO("spike_trap(%d,%d): FIRED, up for %d turn(s)",
                 self->col, self->row, d->up_turns);
    }
}

static bool spike_is_hazardous(const Feature* self, const Grid* grid,
                                int col, int row) {
    (void)grid; (void)col; (void)row;
    const SpikeTrapData* d = (const SpikeTrapData*)self->data;
    return d->active_remaining > 0;
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
    .on_leave              = NULL,
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

    /* Defaults */
    d->up_turns         = 1;
    d->active_remaining = 0;
    d->arm_turn         = -1;

    /* Parse config */
    if (config) {
        const cJSON* active_item = cJSON_GetObjectItemCaseSensitive(config, "initial_active");
        if (cJSON_IsBool(active_item) && cJSON_IsTrue(active_item)) {
            d->active_remaining = d->up_turns;
        }

        const cJSON* up_item = cJSON_GetObjectItemCaseSensitive(config, "up_turns");
        if (cJSON_IsNumber(up_item) && up_item->valueint >= 1) {
            d->up_turns = up_item->valueint;
            /* Re-apply if initially active so it uses the configured value */
            if (d->active_remaining > 0) {
                d->active_remaining = d->up_turns;
            }
        }
    }

    f->data = d;
    return f;
}
