#include "conveyor.h"
#include "../grid.h"
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
            grid_move_entity(grid, ENTITY_THESEUS, dir);
            s_theseus_conveyed_turn = grid->turn_count;
        }
    }

    /* Check if Minotaur is on this tile and hasn't been conveyed this turn */
    if (grid->minotaur_col == self->col && grid->minotaur_row == self->row &&
        s_minotaur_conveyed_turn != grid->turn_count) {
        if (grid_can_move(grid, ENTITY_MINOTAUR, self->col, self->row, dir)) {
            grid_move_entity(grid, ENTITY_MINOTAUR, dir);
            s_minotaur_conveyed_turn = grid->turn_count;
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
