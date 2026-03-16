#include "conveyor.h"
#include "../grid.h"
#include "../turn.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Direction direction;   /* which way the conveyor pushes */
} ConveyorData;

/* ── Anti-chaining ────────────────────────────────────── */

/*
 * Each actor may only be moved by ONE conveyor per environment phase.
 * Track the turn count at which each actor was last conveyed; if it
 * matches the current turn, skip that actor.
 */
static int s_theseus_conveyed_turn  = -1;
static int s_minotaur_conveyed_turn = -1;

/* ── Vtable hooks ─────────────────────────────────────── */

static void conv_on_environment_phase(Feature* self, Grid* grid) {
    ConveyorData* d = (ConveyorData*)self->data;
    Direction dir = d->direction;

    /* Check if Theseus is on this tile and hasn't been conveyed this turn */
    if (grid->theseus_col == self->col && grid->theseus_row == self->row &&
        s_theseus_conveyed_turn != grid->turn_count) {
        if (grid_can_move(grid, ENTITY_THESEUS, self->col, self->row, dir)) {
            int from_c = grid->theseus_col, from_r = grid->theseus_row;
            grid_move_entity(grid, ENTITY_THESEUS, dir);
            s_theseus_conveyed_turn = grid->turn_count;

            AnimEvent evt = {
                .type     = ANIM_EVT_CONVEYOR_PUSH,
                .phase    = ANIM_EVENT_PHASE_ENVIRONMENT,
                .from_col = from_c,
                .from_row = from_r,
                .to_col   = grid->theseus_col,
                .to_row   = grid->theseus_row,
                .entity   = ENTITY_THESEUS,
            };
            evt.conveyor.direction = dir;
            turn_record_push_event(grid->active_record, &evt);
        }
    }

    /* Check if Minotaur is on this tile and hasn't been conveyed this turn */
    if (grid->minotaur_col == self->col && grid->minotaur_row == self->row &&
        s_minotaur_conveyed_turn != grid->turn_count) {
        if (grid_can_move(grid, ENTITY_MINOTAUR, self->col, self->row, dir)) {
            int from_c = grid->minotaur_col, from_r = grid->minotaur_row;
            grid_move_entity(grid, ENTITY_MINOTAUR, dir);
            s_minotaur_conveyed_turn = grid->turn_count;

            AnimEvent evt = {
                .type     = ANIM_EVT_CONVEYOR_PUSH,
                .phase    = ANIM_EVENT_PHASE_ENVIRONMENT,
                .from_col = from_c,
                .from_row = from_r,
                .to_col   = grid->minotaur_col,
                .to_row   = grid->minotaur_row,
                .entity   = ENTITY_MINOTAUR,
            };
            evt.conveyor.direction = dir;
            turn_record_push_event(grid->active_record, &evt);
        }
    }
}

/* No mutable state — snapshot not needed */

static void conv_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable conveyor_vt = {
    .name                  = "conveyor",
    .blocks_movement       = NULL,
    .on_pre_move           = NULL,
    .on_enter              = NULL,
    .on_leave              = NULL,
    .on_push               = NULL,
    .on_environment_phase  = conv_on_environment_phase,
    .is_hazardous          = NULL,
    .snapshot_size          = NULL,
    .snapshot_save          = NULL,
    .snapshot_restore       = NULL,
    .destroy               = conv_destroy,
};

Direction conveyor_get_direction(const Feature* f) {
    if (!f || f->vt != &conveyor_vt || !f->data) return DIR_NONE;
    return ((const ConveyorData*)f->data)->direction;
}

/* ── Factory ──────────────────────────────────────────── */

static Direction parse_dir(const char* s) {
    if (!s) return DIR_NONE;
    if (strcmp(s, "north") == 0) return DIR_NORTH;
    if (strcmp(s, "south") == 0) return DIR_SOUTH;
    if (strcmp(s, "east") == 0)  return DIR_EAST;
    if (strcmp(s, "west") == 0)  return DIR_WEST;
    return DIR_NONE;
}

Feature* conveyor_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&conveyor_vt, col, row);
    if (!f) return NULL;

    ConveyorData* d = calloc(1, sizeof(ConveyorData));
    if (!d) { feature_free(f); return NULL; }

    d->direction = DIR_EAST;  /* default */

    if (config) {
        const cJSON* dir_item = cJSON_GetObjectItemCaseSensitive(config, "direction");
        if (cJSON_IsString(dir_item)) {
            Direction parsed = parse_dir(dir_item->valuestring);
            if (parsed != DIR_NONE) d->direction = parsed;
        }
    }

    f->data = d;
    return f;
}
