#include "undo.h"
#include "../engine/utils.h"
#include <stdlib.h>
#include <string.h>

/* ── Internal helpers ──────────────────────────────────── */

/*
 * Calculate total snapshot blob size for all features with state.
 */
static size_t calc_feature_blob_size(const Grid* grid) {
    size_t total = 0;
    for (int i = 0; i < grid->feature_count; i++) {
        const Feature* f = grid->features[i];
        if (f->vt->snapshot_size) {
            total += f->vt->snapshot_size(f);
        }
    }
    return total;
}

/*
 * Capture full state into a snapshot.
 */
static void snapshot_capture(UndoSnapshot* snap, const Grid* grid) {
    snap->theseus_col  = grid->theseus_col;
    snap->theseus_row  = grid->theseus_row;
    snap->minotaur_col = grid->minotaur_col;
    snap->minotaur_row = grid->minotaur_row;
    snap->turn_count   = grid->turn_count;
    snap->level_won    = grid->level_won;
    snap->level_lost   = grid->level_lost;

    /* Serialize feature internal state */
    size_t blob_size = calc_feature_blob_size(grid);
    if (blob_size > 0) {
        snap->feature_blob = malloc(blob_size);
        snap->feature_blob_size = blob_size;

        uint8_t* ptr = (uint8_t*)snap->feature_blob;
        for (int i = 0; i < grid->feature_count; i++) {
            const Feature* f = grid->features[i];
            if (f->vt->snapshot_size) {
                size_t sz = f->vt->snapshot_size(f);
                if (f->vt->snapshot_save) {
                    f->vt->snapshot_save(f, ptr);
                }
                ptr += sz;
            }
        }
    } else {
        snap->feature_blob = NULL;
        snap->feature_blob_size = 0;
    }

    /* Snapshot cell state (walls + impassable) */
    int total_cells = grid->cols * grid->rows;
    snap->cell_snapshots = malloc((size_t)total_cells * sizeof(CellSnapshot));
    snap->cell_count = total_cells;
    for (int i = 0; i < total_cells; i++) {
        const Cell* c = &grid->cells[i];
        for (int d = 0; d < DIR_COUNT; d++) {
            snap->cell_snapshots[i].walls[d] = c->walls[d];
        }
        snap->cell_snapshots[i].impassable = c->impassable;
    }

    /* Snapshot feature positions (col/row) */
    snap->feature_pos_count = grid->feature_count;
    if (grid->feature_count > 0) {
        snap->feature_positions = malloc((size_t)(grid->feature_count * 2) * sizeof(int));
        for (int i = 0; i < grid->feature_count; i++) {
            snap->feature_positions[i * 2]     = grid->features[i]->col;
            snap->feature_positions[i * 2 + 1] = grid->features[i]->row;
        }
    } else {
        snap->feature_positions = NULL;
    }
}

/*
 * Restore grid state from a snapshot.
 */
static void snapshot_restore(const UndoSnapshot* snap, Grid* grid) {
    grid->theseus_col  = snap->theseus_col;
    grid->theseus_row  = snap->theseus_row;
    grid->minotaur_col = snap->minotaur_col;
    grid->minotaur_row = snap->minotaur_row;
    grid->turn_count   = snap->turn_count;
    grid->level_won    = snap->level_won;
    grid->level_lost   = snap->level_lost;

    /* Restore feature internal state */
    if (snap->feature_blob && snap->feature_blob_size > 0) {
        const uint8_t* ptr = (const uint8_t*)snap->feature_blob;
        for (int i = 0; i < grid->feature_count; i++) {
            Feature* f = grid->features[i];
            if (f->vt->snapshot_size) {
                size_t sz = f->vt->snapshot_size(f);
                if (f->vt->snapshot_restore) {
                    f->vt->snapshot_restore(f, ptr);
                }
                ptr += sz;
            }
        }
    }

    /* Restore cell state (walls + impassable) */
    if (snap->cell_snapshots && snap->cell_count > 0) {
        int total_cells = grid->cols * grid->rows;
        int count = (snap->cell_count < total_cells) ? snap->cell_count : total_cells;
        for (int i = 0; i < count; i++) {
            Cell* c = &grid->cells[i];
            for (int d = 0; d < DIR_COUNT; d++) {
                c->walls[d] = snap->cell_snapshots[i].walls[d];
            }
            c->impassable = snap->cell_snapshots[i].impassable;
        }
    }

    /* Restore feature positions and rebuild cell links */
    if (snap->feature_positions && snap->feature_pos_count > 0) {
        int count = (snap->feature_pos_count < grid->feature_count)
                    ? snap->feature_pos_count : grid->feature_count;
        for (int i = 0; i < count; i++) {
            grid->features[i]->col = snap->feature_positions[i * 2];
            grid->features[i]->row = snap->feature_positions[i * 2 + 1];
        }
    }

    /* Rebuild cell->features links from feature col/row */
    grid_rebuild_feature_links(grid);
}

/*
 * Free all allocated memory in a snapshot (but not the struct itself).
 */
static void snapshot_free_blob(UndoSnapshot* snap) {
    free(snap->feature_blob);
    snap->feature_blob = NULL;
    snap->feature_blob_size = 0;

    free(snap->cell_snapshots);
    snap->cell_snapshots = NULL;
    snap->cell_count = 0;

    free(snap->feature_positions);
    snap->feature_positions = NULL;
    snap->feature_pos_count = 0;
}

/* ── Public API ────────────────────────────────────────── */

void undo_init(UndoStack* stack) {
    memset(stack, 0, sizeof(UndoStack));
}

void undo_clear(UndoStack* stack) {
    for (int i = 0; i < stack->count; i++) {
        snapshot_free_blob(&stack->snapshots[i]);
    }
    stack->count = 0;

    if (stack->has_initial) {
        snapshot_free_blob(&stack->initial);
        stack->has_initial = false;
    }
}

void undo_save_initial(UndoStack* stack, const Grid* grid) {
    if (stack->has_initial) {
        snapshot_free_blob(&stack->initial);
    }
    snapshot_capture(&stack->initial, grid);
    stack->has_initial = true;
}

void undo_push(UndoStack* stack, const Grid* grid) {
    if (stack->count >= UNDO_MAX_DEPTH) {
        /* Drop oldest snapshot to make room */
        snapshot_free_blob(&stack->snapshots[0]);
        memmove(&stack->snapshots[0], &stack->snapshots[1],
                (size_t)(UNDO_MAX_DEPTH - 1) * sizeof(UndoSnapshot));
        stack->count = UNDO_MAX_DEPTH - 1;
    }

    snapshot_capture(&stack->snapshots[stack->count], grid);
    stack->count++;
}

bool undo_pop(UndoStack* stack, Grid* grid) {
    if (stack->count <= 0) {
        LOG_DEBUG("undo_pop: nothing to undo");
        return false;
    }

    stack->count--;
    UndoSnapshot* snap = &stack->snapshots[stack->count];
    snapshot_restore(snap, grid);
    snapshot_free_blob(snap);

    return true;
}

bool undo_reset(UndoStack* stack, Grid* grid) {
    if (!stack->has_initial) {
        LOG_WARN("undo_reset: no initial state saved");
        return false;
    }

    /* Restore initial state */
    snapshot_restore(&stack->initial, grid);

    /* Clear undo stack (but keep initial) */
    for (int i = 0; i < stack->count; i++) {
        snapshot_free_blob(&stack->snapshots[i]);
    }
    stack->count = 0;

    return true;
}

int undo_depth(const UndoStack* stack) {
    return stack->count;
}
