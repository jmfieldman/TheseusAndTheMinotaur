#include "grid.h"
#include "../engine/utils.h"
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────── */

Grid* grid_create(int cols, int rows) {
    if (cols < 1 || rows < 1 || cols > 16 || rows > 16) {
        LOG_ERROR("grid_create: invalid dimensions %dx%d", cols, rows);
        return NULL;
    }

    Grid* g = calloc(1, sizeof(Grid));
    if (!g) return NULL;

    g->cols = cols;
    g->rows = rows;
    g->cells = calloc((size_t)(cols * rows), sizeof(Cell));
    if (!g->cells) {
        free(g);
        return NULL;
    }

    /* Set boundary walls */
    for (int c = 0; c < cols; c++) {
        grid_cell(g, c, 0)->walls[DIR_SOUTH]      = true;   /* bottom edge */
        grid_cell(g, c, rows - 1)->walls[DIR_NORTH] = true; /* top edge */
    }
    for (int r = 0; r < rows; r++) {
        grid_cell(g, 0, r)->walls[DIR_WEST]        = true;  /* left edge */
        grid_cell(g, cols - 1, r)->walls[DIR_EAST]  = true;  /* right edge */
    }

    return g;
}

void grid_destroy(Grid* grid) {
    if (!grid) return;

    /* Destroy all owned features */
    for (int i = 0; i < grid->feature_count; i++) {
        feature_free(grid->features[i]);
    }

    free(grid->cells);
    free(grid);
}

/* ── Cell access ───────────────────────────────────────── */

bool grid_in_bounds(const Grid* grid, int col, int row) {
    return col >= 0 && col < grid->cols && row >= 0 && row < grid->rows;
}

Cell* grid_cell(Grid* grid, int col, int row) {
    if (!grid_in_bounds(grid, col, row)) return NULL;
    return &grid->cells[row * grid->cols + col];
}

const Cell* grid_cell_const(const Grid* grid, int col, int row) {
    if (!grid_in_bounds(grid, col, row)) return NULL;
    return &grid->cells[row * grid->cols + col];
}

/* ── Walls ─────────────────────────────────────────────── */

void grid_set_wall(Grid* grid, int col, int row, Direction side, bool present) {
    Cell* cell = grid_cell(grid, col, row);
    if (!cell) return;

    cell->walls[side] = present;

    /* Mirror on neighbour */
    int nc = col + direction_dcol(side);
    int nr = row + direction_drow(side);
    Cell* neighbour = grid_cell(grid, nc, nr);
    if (neighbour) {
        neighbour->walls[direction_opposite(side)] = present;
    }
}

bool grid_has_wall(const Grid* grid, int col, int row, Direction side) {
    const Cell* cell = grid_cell_const(grid, col, row);
    if (!cell) return true;   /* out of bounds = walled */
    return cell->walls[side];
}

/* ── Movement queries ──────────────────────────────────── */

bool grid_can_move(const Grid* grid, EntityID who,
                   int col, int row, Direction dir) {
    /* Wall blocks? */
    if (grid_has_wall(grid, col, row, dir)) {
        return false;
    }

    int tc = col + direction_dcol(dir);
    int tr = row + direction_drow(dir);

    /* Out of bounds? (shouldn't happen if walls are correct, but safety) */
    if (!grid_in_bounds(grid, tc, tr)) {
        return false;
    }

    /* Impassable tile? */
    const Cell* target = grid_cell_const(grid, tc, tr);
    if (target->impassable) {
        return false;
    }

    /* Check all feature blocks_movement hooks on the TARGET cell */
    for (int i = 0; i < target->feature_count; i++) {
        const Feature* f = target->features[i];
        if (f->vt->blocks_movement &&
            f->vt->blocks_movement(f, grid, who, col, row, tc, tr)) {
            return false;
        }
    }

    /* Also check features on the SOURCE cell (e.g. one-way gates) */
    const Cell* source = grid_cell_const(grid, col, row);
    for (int i = 0; i < source->feature_count; i++) {
        const Feature* f = source->features[i];
        if (f->vt->blocks_movement &&
            f->vt->blocks_movement(f, grid, who, col, row, tc, tr)) {
            return false;
        }
    }

    return true;
}

bool grid_move_entity(Grid* grid, EntityID who, Direction dir) {
    int col, row;
    grid_get_entity_pos(grid, who, &col, &row);

    if (!grid_can_move(grid, who, col, row, dir)) {
        return false;
    }

    int nc = col + direction_dcol(dir);
    int nr = row + direction_drow(dir);

    /* Update position first so hooks can query the entity's new location */
    grid_set_entity_pos(grid, who, nc, nr);

    /* Fire on_leave for features at old position */
    Cell* old_cell = grid_cell(grid, col, row);
    for (int i = 0; i < old_cell->feature_count; i++) {
        Feature* f = old_cell->features[i];
        if (f->vt->on_leave) {
            f->vt->on_leave(f, grid, who, col, row);
        }
    }

    /* Fire on_enter for features at new position */
    Cell* new_cell = grid_cell(grid, nc, nr);
    for (int i = 0; i < new_cell->feature_count; i++) {
        Feature* f = new_cell->features[i];
        if (f->vt->on_enter) {
            f->vt->on_enter(f, grid, who, nc, nr);
        }
    }

    return true;
}

/* ── Feature management ────────────────────────────────── */

bool grid_add_feature(Grid* grid, Feature* feature) {
    if (!feature) return false;

    if (grid->feature_count >= MAX_FEATURES) {
        LOG_ERROR("grid_add_feature: max features (%d) exceeded", MAX_FEATURES);
        return false;
    }

    Cell* cell = grid_cell(grid, feature->col, feature->row);
    if (!cell) {
        LOG_ERROR("grid_add_feature: feature at (%d,%d) is out of bounds",
                  feature->col, feature->row);
        return false;
    }

    if (cell->feature_count >= MAX_FEATURES_PER_CELL) {
        LOG_ERROR("grid_add_feature: cell (%d,%d) full (%d features)",
                  feature->col, feature->row, MAX_FEATURES_PER_CELL);
        return false;
    }

    /* Add to grid-level array */
    grid->features[grid->feature_count++] = feature;

    /* Add to cell-level array */
    cell->features[cell->feature_count++] = feature;

    return true;
}

int grid_get_features_at(const Grid* grid, int col, int row,
                         Feature** out_features, int max_out) {
    const Cell* cell = grid_cell_const(grid, col, row);
    if (!cell) return 0;

    int count = MIN(cell->feature_count, max_out);
    for (int i = 0; i < count; i++) {
        out_features[i] = cell->features[i];
    }
    return count;
}

/* ── Feature link rebuilding ───────────────────────────── */

void grid_rebuild_feature_links(Grid* grid) {
    /* Clear all cell-level feature arrays */
    int total = grid->cols * grid->rows;
    for (int i = 0; i < total; i++) {
        grid->cells[i].feature_count = 0;
    }

    /* Re-link each feature to its cell based on current col/row */
    for (int i = 0; i < grid->feature_count; i++) {
        Feature* f = grid->features[i];
        Cell* cell = grid_cell(grid, f->col, f->row);
        if (cell && cell->feature_count < MAX_FEATURES_PER_CELL) {
            cell->features[cell->feature_count++] = f;
        }
    }
}

/* ── Entity position helpers ───────────────────────────── */

void grid_get_entity_pos(const Grid* grid, EntityID who,
                         int* out_col, int* out_row) {
    if (who == ENTITY_THESEUS) {
        *out_col = grid->theseus_col;
        *out_row = grid->theseus_row;
    } else {
        *out_col = grid->minotaur_col;
        *out_row = grid->minotaur_row;
    }
}

void grid_set_entity_pos(Grid* grid, EntityID who, int col, int row) {
    if (who == ENTITY_THESEUS) {
        grid->theseus_col = col;
        grid->theseus_row = row;
    } else {
        grid->minotaur_col = col;
        grid->minotaur_row = row;
    }
}

/* ── Exit / win / loss helpers ─────────────────────────── */

bool grid_theseus_at_exit(const Grid* grid) {
    return (grid->theseus_col == grid->exit_col &&
            grid->theseus_row == grid->exit_row);
}

bool grid_entities_collide(const Grid* grid) {
    return (grid->theseus_col == grid->minotaur_col &&
            grid->theseus_row == grid->minotaur_row);
}

bool grid_theseus_on_hazard(const Grid* grid) {
    const Cell* cell = grid_cell_const(grid,
                                       grid->theseus_col,
                                       grid->theseus_row);
    if (!cell) return false;

    for (int i = 0; i < cell->feature_count; i++) {
        const Feature* f = cell->features[i];
        if (f->vt->is_hazardous &&
            f->vt->is_hazardous(f, grid, grid->theseus_col, grid->theseus_row)) {
            return true;
        }
    }
    return false;
}
