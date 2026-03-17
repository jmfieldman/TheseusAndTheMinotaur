#include "teleporter.h"
#include "../grid.h"
#include "../turn.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char pair_id[32];
    bool teleporting;       /* re-entrancy guard to prevent chaining */
} TeleporterData;

/* ── Vtable hooks ─────────────────────────────────────── */

static void tp_on_enter(Feature* self, Grid* grid, EntityID who,
                         int col, int row) {
    (void)col; (void)row;
    TeleporterData* d = (TeleporterData*)self->data;

    /* Guard against chaining: if we're already mid-teleport, skip */
    if (d->teleporting) return;

    /* Find the paired teleporter */
    for (int i = 0; i < grid->feature_count; i++) {
        Feature* other = grid->features[i];
        if (other == self) continue;
        if (strcmp(other->vt->name, "teleporter") != 0) continue;

        TeleporterData* od = (TeleporterData*)other->data;
        if (strcmp(od->pair_id, d->pair_id) != 0) continue;

        /* Found the pair — teleport the actor */
        d->teleporting = true;
        od->teleporting = true;

        /* Record teleport animation event */
        if (who == ENTITY_THESEUS) {
            AnimEvent evt = {
                .type     = ANIM_EVT_THESEUS_TELEPORT,
                .phase    = ANIM_EVENT_PHASE_THESEUS,
                .from_col = self->col,
                .from_row = self->row,
                .to_col   = other->col,
                .to_row   = other->row,
                .entity   = ENTITY_THESEUS,
            };
            turn_record_push_event(grid->active_record, &evt);
        } else if (who == ENTITY_MINOTAUR) {
            AnimEvent evt = {
                .type     = ANIM_EVT_MINOTAUR_TELEPORT,
                .phase    = ANIM_EVENT_PHASE_THESEUS,  /* phase field unused for mino; stored for event scanning */
                .from_col = self->col,
                .from_row = self->row,
                .to_col   = other->col,
                .to_row   = other->row,
                .entity   = ENTITY_MINOTAUR,
            };
            turn_record_push_event(grid->active_record, &evt);
        }

        grid_set_entity_pos(grid, who, other->col, other->row);

        /* Fire on_enter hooks at the destination (except teleporters,
         * which are guarded by the teleporting flag) */
        Cell* dest_cell = grid_cell(grid, other->col, other->row);
        if (dest_cell) {
            for (int j = 0; j < dest_cell->feature_count; j++) {
                Feature* df = dest_cell->features[j];
                if (df->vt->on_enter && df != other) {
                    df->vt->on_enter(df, grid, who, other->col, other->row);
                }
            }
        }

        d->teleporting = false;
        od->teleporting = false;
        break;
    }
}

static size_t tp_snapshot_size(const Feature* self) {
    (void)self;
    return 0;   /* no mutable state to snapshot */
}

static void tp_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable teleporter_vt = {
    .name                  = "teleporter",
    .blocks_movement       = NULL,
    .on_pre_move           = NULL,
    .on_enter              = tp_on_enter,
    .on_leave              = NULL,
    .on_push               = NULL,
    .on_environment_phase  = NULL,
    .is_hazardous          = NULL,
    .snapshot_size          = tp_snapshot_size,
    .snapshot_save          = NULL,
    .snapshot_restore       = NULL,
    .destroy               = tp_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* teleporter_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&teleporter_vt, col, row);
    if (!f) return NULL;

    TeleporterData* d = calloc(1, sizeof(TeleporterData));
    if (!d) { feature_free(f); return NULL; }

    strncpy(d->pair_id, "default", sizeof(d->pair_id) - 1);
    d->teleporting = false;

    if (config) {
        const cJSON* id_item = cJSON_GetObjectItemCaseSensitive(config, "pair_id");
        if (cJSON_IsString(id_item)) {
            strncpy(d->pair_id, id_item->valuestring, sizeof(d->pair_id) - 1);
        }
    }

    f->data = d;
    return f;
}
