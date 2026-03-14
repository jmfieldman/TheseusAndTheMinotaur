#include "groove_box.h"
#include "../grid.h"
#include "../turn.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GROOVE_LENGTH 32

typedef struct {
    int groove_cols[MAX_GROOVE_LENGTH];
    int groove_rows[MAX_GROOVE_LENGTH];
    int groove_length;
    int box_col, box_row;   /* current box position */
} GrooveBoxData;

/* ── Helpers ──────────────────────────────────────────── */

static bool is_on_groove(const GrooveBoxData* d, int col, int row) {
    for (int i = 0; i < d->groove_length; i++) {
        if (d->groove_cols[i] == col && d->groove_rows[i] == row) {
            return true;
        }
    }
    return false;
}

static bool is_groove_direction(const GrooveBoxData* d, int from_col, int from_row,
                                 Direction dir) {
    int tc = from_col + direction_dcol(dir);
    int tr = from_row + direction_drow(dir);
    return is_on_groove(d, tc, tr);
}

/* ── Vtable hooks ─────────────────────────────────────── */

static bool gb_blocks_movement(const Feature* self, const Grid* grid,
                                EntityID who,
                                int from_col, int from_row,
                                int to_col, int to_row) {
    (void)grid; (void)who; (void)from_col; (void)from_row;
    const GrooveBoxData* d = (const GrooveBoxData*)self->data;

    /* The box blocks movement into its current position */
    return (to_col == d->box_col && to_row == d->box_row);
}

static bool gb_on_push(Feature* self, Grid* grid,
                        int from_col, int from_row, Direction dir) {
    GrooveBoxData* d = (GrooveBoxData*)self->data;

    /* Check if the push is aimed at the box */
    int push_target_col = from_col + direction_dcol(dir);
    int push_target_row = from_row + direction_drow(dir);
    if (push_target_col != d->box_col || push_target_row != d->box_row) {
        return false;
    }

    /* Check if the push direction is aligned with the groove */
    int box_dest_col = d->box_col + direction_dcol(dir);
    int box_dest_row = d->box_row + direction_drow(dir);

    if (!is_on_groove(d, box_dest_col, box_dest_row)) {
        return false;   /* off groove — blocked */
    }

    /* Check for wall between box and destination */
    if (grid_has_wall(grid, d->box_col, d->box_row, dir)) {
        return false;
    }

    /* Check if destination is impassable */
    Cell* dest = grid_cell(grid, box_dest_col, box_dest_row);
    if (!dest || dest->impassable) return false;

    /* Check if another groove box is at the destination */
    for (int i = 0; i < dest->feature_count; i++) {
        Feature* f = dest->features[i];
        if (f != self && strcmp(f->vt->name, "groove_box") == 0) {
            GrooveBoxData* od = (GrooveBoxData*)f->data;
            if (od->box_col == box_dest_col && od->box_row == box_dest_row) {
                return false;   /* another box in the way */
            }
        }
    }

    /* Record animation events before the move */
    {
        AnimEvent box_evt = {
            .type     = ANIM_EVT_BOX_SLIDE,
            .phase    = ANIM_EVENT_PHASE_THESEUS,
            .from_col = d->box_col,
            .from_row = d->box_row,
            .to_col   = box_dest_col,
            .to_row   = box_dest_row,
            .entity   = ENTITY_THESEUS,
        };
        box_evt.box.box_from_col = d->box_col;
        box_evt.box.box_from_row = d->box_row;
        box_evt.box.box_to_col   = box_dest_col;
        box_evt.box.box_to_row   = box_dest_row;
        turn_record_push_event(grid->active_record, &box_evt);

        AnimEvent push_evt = {
            .type     = ANIM_EVT_THESEUS_PUSH_MOVE,
            .phase    = ANIM_EVENT_PHASE_THESEUS,
            .from_col = from_col,
            .from_row = from_row,
            .to_col   = push_target_col,
            .to_row   = push_target_row,
            .entity   = ENTITY_THESEUS,
        };
        turn_record_push_event(grid->active_record, &push_evt);
    }

    /* Push succeeds — move the box */
    d->box_col = box_dest_col;
    d->box_row = box_dest_row;

    /* Keep feature grid position in sync so cell feature links are correct */
    self->col = box_dest_col;
    self->row = box_dest_row;
    grid_rebuild_feature_links(grid);

    /* Move Theseus into the vacated tile */
    /* Fire on_leave for features at Theseus's current position */
    Cell* old_cell = grid_cell(grid, from_col, from_row);
    if (old_cell) {
        for (int i = 0; i < old_cell->feature_count; i++) {
            Feature* f = old_cell->features[i];
            if (f->vt->on_leave) {
                f->vt->on_leave(f, grid, ENTITY_THESEUS, from_col, from_row);
            }
        }
    }

    grid_set_entity_pos(grid, ENTITY_THESEUS, push_target_col, push_target_row);

    /* Fire on_enter for features at new position (excluding this box) */
    Cell* new_cell = grid_cell(grid, push_target_col, push_target_row);
    if (new_cell) {
        for (int i = 0; i < new_cell->feature_count; i++) {
            Feature* f = new_cell->features[i];
            if (f != self && f->vt->on_enter) {
                f->vt->on_enter(f, grid, ENTITY_THESEUS,
                                push_target_col, push_target_row);
            }
        }
    }

    return true;
}

static size_t gb_snapshot_size(const Feature* self) {
    (void)self;
    return sizeof(int) * 2;  /* box_col, box_row */
}

static void gb_snapshot_save(const Feature* self, void* buf) {
    const GrooveBoxData* d = (const GrooveBoxData*)self->data;
    int* p = (int*)buf;
    p[0] = d->box_col;
    p[1] = d->box_row;
}

static void gb_snapshot_restore(Feature* self, const void* buf) {
    GrooveBoxData* d = (GrooveBoxData*)self->data;
    const int* p = (const int*)buf;
    d->box_col = p[0];
    d->box_row = p[1];
    /* Keep feature grid position in sync */
    self->col = p[0];
    self->row = p[1];
}

static void gb_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable groove_box_vt = {
    .name                  = "groove_box",
    .blocks_movement       = gb_blocks_movement,
    .on_pre_move           = NULL,
    .on_enter              = NULL,
    .on_leave              = NULL,
    .on_push               = gb_on_push,
    .on_environment_phase  = NULL,
    .is_hazardous          = NULL,
    .snapshot_size          = gb_snapshot_size,
    .snapshot_save          = gb_snapshot_save,
    .snapshot_restore       = gb_snapshot_restore,
    .destroy               = gb_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

int groove_box_get_path(const Feature* f, int* out_cols, int* out_rows, int max_out) {
    if (!f || !f->vt || !f->vt->name) return 0;
    if (strcmp(f->vt->name, "groove_box") != 0) return 0;
    const GrooveBoxData* d = (const GrooveBoxData*)f->data;
    if (!d) return 0;
    int count = d->groove_length < max_out ? d->groove_length : max_out;
    for (int i = 0; i < count; i++) {
        out_cols[i] = d->groove_cols[i];
        out_rows[i] = d->groove_rows[i];
    }
    return count;
}

Feature* groove_box_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&groove_box_vt, col, row);
    if (!f) return NULL;

    GrooveBoxData* d = calloc(1, sizeof(GrooveBoxData));
    if (!d) { feature_free(f); return NULL; }

    d->box_col = col;
    d->box_row = row;

    if (config) {
        /* Parse groove track */
        const cJSON* groove_arr = cJSON_GetObjectItemCaseSensitive(config, "groove");
        if (cJSON_IsArray(groove_arr)) {
            const cJSON* item = NULL;
            int idx = 0;
            cJSON_ArrayForEach(item, groove_arr) {
                if (idx >= MAX_GROOVE_LENGTH) break;
                const cJSON* c_item = cJSON_GetObjectItemCaseSensitive(item, "col");
                const cJSON* r_item = cJSON_GetObjectItemCaseSensitive(item, "row");
                if (cJSON_IsNumber(c_item) && cJSON_IsNumber(r_item)) {
                    d->groove_cols[idx] = c_item->valueint;
                    d->groove_rows[idx] = r_item->valueint;
                    idx++;
                }
            }
            d->groove_length = idx;
        }

        /* Parse initial box position */
        const cJSON* pos_item = cJSON_GetObjectItemCaseSensitive(config, "initial_pos");
        if (pos_item) {
            const cJSON* c_item = cJSON_GetObjectItemCaseSensitive(pos_item, "col");
            const cJSON* r_item = cJSON_GetObjectItemCaseSensitive(pos_item, "row");
            if (cJSON_IsNumber(c_item)) d->box_col = c_item->valueint;
            if (cJSON_IsNumber(r_item)) d->box_row = r_item->valueint;
        }
    }

    f->data = d;
    return f;
}
