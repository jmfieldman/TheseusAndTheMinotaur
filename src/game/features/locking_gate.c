#include "locking_gate.h"
#include "../grid.h"
#include "../turn.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Direction gate_side;    /* which wall edge the gate occupies */
    bool locked;            /* true = bars are up, impassable */
} LockingGateData;

/* ── Vtable hooks ─────────────────────────────────────── */

static bool lg_blocks_movement(const Feature* self, const Grid* grid,
                                EntityID who,
                                int from_col, int from_row,
                                int to_col, int to_row) {
    (void)grid; (void)who;
    const LockingGateData* d = (const LockingGateData*)self->data;
    if (!d->locked) return false;

    /* Block movement through the gate edge in either direction */
    int dc = to_col - from_col;
    int dr = to_row - from_row;

    /* Check if this movement crosses the gate's wall edge */
    if (from_col == self->col && from_row == self->row) {
        /* Moving OUT of the gate cell in the gate's direction */
        if ((d->gate_side == DIR_EAST  && dc == 1  && dr == 0) ||
            (d->gate_side == DIR_WEST  && dc == -1 && dr == 0) ||
            (d->gate_side == DIR_NORTH && dc == 0  && dr == 1) ||
            (d->gate_side == DIR_SOUTH && dc == 0  && dr == -1)) {
            return true;
        }
    }

    return false;
}

static void lg_on_leave(Feature* self, Grid* grid, EntityID who,
                         int col, int row) {
    (void)who; (void)col; (void)row;
    LockingGateData* d = (LockingGateData*)self->data;
    if (d->locked) return;

    /* Check if the actor left through the gate's side */
    int entity_col, entity_row;
    grid_get_entity_pos(grid, who, &entity_col, &entity_row);

    int dc = entity_col - self->col;
    int dr = entity_row - self->row;

    bool left_through_gate = false;
    if (d->gate_side == DIR_EAST  && dc == 1  && dr == 0) left_through_gate = true;
    if (d->gate_side == DIR_WEST  && dc == -1 && dr == 0) left_through_gate = true;
    if (d->gate_side == DIR_NORTH && dc == 0  && dr == 1) left_through_gate = true;
    if (d->gate_side == DIR_SOUTH && dc == 0  && dr == -1) left_through_gate = true;

    if (left_through_gate) {
        d->locked = true;
        grid_set_wall(grid, self->col, self->row, d->gate_side, true);

        /* Record gate lock animation event */
        AnimEvent evt = {
            .type     = ANIM_EVT_GATE_LOCK,
            .phase    = ANIM_EVENT_PHASE_THESEUS_EFFECT,
            .from_col = self->col,
            .from_row = self->row,
            .to_col   = self->col,
            .to_row   = self->row,
        };
        evt.gate.gate_side = d->gate_side;
        turn_record_push_event(grid->active_record, &evt);
    }
}

static size_t lg_snapshot_size(const Feature* self) {
    (void)self;
    return sizeof(bool);
}

static void lg_snapshot_save(const Feature* self, void* buf) {
    const LockingGateData* d = (const LockingGateData*)self->data;
    memcpy(buf, &d->locked, sizeof(bool));
}

static void lg_snapshot_restore(Feature* self, const void* buf) {
    LockingGateData* d = (LockingGateData*)self->data;
    memcpy(&d->locked, buf, sizeof(bool));
    /* Wall state is restored by undo cell snapshot */
}

static void lg_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable locking_gate_vt = {
    .name                  = "locking_gate",
    .blocks_movement       = lg_blocks_movement,
    .on_pre_move           = NULL,
    .on_enter              = NULL,
    .on_leave              = lg_on_leave,
    .on_push               = NULL,
    .on_environment_phase  = NULL,
    .is_hazardous          = NULL,
    .snapshot_size          = lg_snapshot_size,
    .snapshot_save          = lg_snapshot_save,
    .snapshot_restore       = lg_snapshot_restore,
    .destroy               = lg_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

static Direction parse_dir(const char* s) {
    if (!s) return DIR_NONE;
    if (strcmp(s, "north") == 0) return DIR_NORTH;
    if (strcmp(s, "south") == 0) return DIR_SOUTH;
    if (strcmp(s, "east") == 0) return DIR_EAST;
    if (strcmp(s, "west") == 0) return DIR_WEST;
    return DIR_NONE;
}

Feature* locking_gate_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&locking_gate_vt, col, row);
    if (!f) return NULL;

    LockingGateData* d = calloc(1, sizeof(LockingGateData));
    if (!d) { feature_free(f); return NULL; }

    d->gate_side = DIR_EAST;
    d->locked = false;

    if (config) {
        const cJSON* side_item = cJSON_GetObjectItemCaseSensitive(config, "side");
        if (cJSON_IsString(side_item)) {
            d->gate_side = parse_dir(side_item->valuestring);
        }
    }

    f->data = d;
    return f;
}
