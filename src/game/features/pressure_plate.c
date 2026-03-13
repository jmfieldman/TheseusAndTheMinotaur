#include "pressure_plate.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

/* ── Target types ─────────────────────────────────────── */

typedef enum {
    PP_TARGET_WALL,
    PP_TARGET_TILE
} PPTargetType;

typedef struct {
    PPTargetType type;
    int col, row;
    Direction side;         /* for wall targets */
    bool initial_active;    /* wall present / tile impassable when active */
} PPTarget;

/* Plate action: what happens when Theseus steps on the plate */
typedef enum {
    PP_ACTION_TOGGLE,   /* flip state each step (default) */
    PP_ACTION_SET,      /* always set targets to active (walls up / tiles blocked) */
    PP_ACTION_CLEAR     /* always clear targets (walls down / tiles passable) */
} PPAction;

typedef struct {
    PPTarget* targets;
    int target_count;
    bool toggled;           /* false = initial state, true = toggled */
    PPAction action;
    char color[16];
} PressurePlateData;

/* ── Helpers ──────────────────────────────────────────── */

static void apply_targets(const PressurePlateData* d, Grid* grid) {
    for (int i = 0; i < d->target_count; i++) {
        const PPTarget* t = &d->targets[i];
        /* Current state: initial_active XOR toggled */
        bool active = t->initial_active != d->toggled;

        if (t->type == PP_TARGET_WALL) {
            grid_set_wall(grid, t->col, t->row, t->side, active);
        } else {
            Cell* cell = grid_cell(grid, t->col, t->row);
            if (cell) {
                cell->impassable = active;
            }
        }
    }
}

/* ── Vtable hooks ─────────────────────────────────────── */

static void pp_on_enter(Feature* self, Grid* grid, EntityID who,
                         int col, int row) {
    (void)col; (void)row;
    if (who != ENTITY_THESEUS) return;

    PressurePlateData* d = (PressurePlateData*)self->data;

    switch (d->action) {
        case PP_ACTION_TOGGLE:
            d->toggled = !d->toggled;
            break;
        case PP_ACTION_SET:
            d->toggled = false;  /* active = initial_active XOR false = initial_active */
            break;
        case PP_ACTION_CLEAR:
            d->toggled = true;   /* active = initial_active XOR true = !initial_active */
            break;
    }

    apply_targets(d, grid);
}

static size_t pp_snapshot_size(const Feature* self) {
    (void)self;
    return sizeof(bool);  /* just the toggled flag */
}

static void pp_snapshot_save(const Feature* self, void* buf) {
    const PressurePlateData* d = (const PressurePlateData*)self->data;
    memcpy(buf, &d->toggled, sizeof(bool));
}

static void pp_snapshot_restore(Feature* self, const void* buf) {
    PressurePlateData* d = (PressurePlateData*)self->data;
    memcpy(&d->toggled, buf, sizeof(bool));
    /* Note: wall/tile state is restored by the undo system's cell snapshot,
     * so we don't need to call apply_targets here. */
}

static void pp_destroy(Feature* self) {
    PressurePlateData* d = (PressurePlateData*)self->data;
    if (d) {
        free(d->targets);
        free(d);
    }
    self->data = NULL;
}

static const FeatureVTable pressure_plate_vt = {
    .name                  = "pressure_plate",
    .blocks_movement       = NULL,
    .on_pre_move           = NULL,
    .on_enter              = pp_on_enter,
    .on_leave              = NULL,
    .on_push               = NULL,
    .on_environment_phase  = NULL,
    .is_hazardous          = NULL,
    .snapshot_size          = pp_snapshot_size,
    .snapshot_save          = pp_snapshot_save,
    .snapshot_restore       = pp_snapshot_restore,
    .destroy               = pp_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* pressure_plate_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&pressure_plate_vt, col, row);
    if (!f) return NULL;

    PressurePlateData* d = calloc(1, sizeof(PressurePlateData));
    if (!d) { feature_free(f); return NULL; }

    d->toggled = false;
    d->action = PP_ACTION_TOGGLE;
    strncpy(d->color, "blue", sizeof(d->color) - 1);

    if (config) {
        const cJSON* color_item = cJSON_GetObjectItemCaseSensitive(config, "color");
        if (cJSON_IsString(color_item)) {
            strncpy(d->color, color_item->valuestring, sizeof(d->color) - 1);
        }

        const cJSON* action_item = cJSON_GetObjectItemCaseSensitive(config, "action");
        if (cJSON_IsString(action_item)) {
            const char* a = action_item->valuestring;
            if (strcmp(a, "set") == 0)        d->action = PP_ACTION_SET;
            else if (strcmp(a, "clear") == 0)  d->action = PP_ACTION_CLEAR;
            else                               d->action = PP_ACTION_TOGGLE;
        }

        const cJSON* targets_arr = cJSON_GetObjectItemCaseSensitive(config, "targets");
        if (cJSON_IsArray(targets_arr)) {
            int count = cJSON_GetArraySize(targets_arr);
            d->targets = calloc((size_t)count, sizeof(PPTarget));
            d->target_count = count;

            int idx = 0;
            const cJSON* item = NULL;
            cJSON_ArrayForEach(item, targets_arr) {
                PPTarget* t = &d->targets[idx++];

                const cJSON* type_item = cJSON_GetObjectItemCaseSensitive(item, "type");
                if (cJSON_IsString(type_item)) {
                    t->type = (strcmp(type_item->valuestring, "wall") == 0)
                              ? PP_TARGET_WALL : PP_TARGET_TILE;
                }

                const cJSON* c_item = cJSON_GetObjectItemCaseSensitive(item, "col");
                if (cJSON_IsNumber(c_item)) t->col = c_item->valueint;

                const cJSON* r_item = cJSON_GetObjectItemCaseSensitive(item, "row");
                if (cJSON_IsNumber(r_item)) t->row = r_item->valueint;

                const cJSON* side_item = cJSON_GetObjectItemCaseSensitive(item, "side");
                if (cJSON_IsString(side_item)) {
                    const char* s = side_item->valuestring;
                    if (strcmp(s, "north") == 0) t->side = DIR_NORTH;
                    else if (strcmp(s, "south") == 0) t->side = DIR_SOUTH;
                    else if (strcmp(s, "east") == 0) t->side = DIR_EAST;
                    else if (strcmp(s, "west") == 0) t->side = DIR_WEST;
                }

                const cJSON* ia_item = cJSON_GetObjectItemCaseSensitive(item, "initial_active");
                t->initial_active = cJSON_IsTrue(ia_item);
            }
        }
    }

    f->data = d;
    return f;
}
