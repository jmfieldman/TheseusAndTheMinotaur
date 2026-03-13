#include "turn.h"
#include "minotaur.h"
#include "../engine/utils.h"
#include <string.h>

/* ── Environment phase ─────────────────────────────────── */

void turn_run_environment_phase(Grid* grid) {
    for (int i = 0; i < grid->feature_count; i++) {
        Feature* f = grid->features[i];
        if (f->vt->on_environment_phase) {
            f->vt->on_environment_phase(f, grid);
        }
    }
}

/* ── Pre-move check ────────────────────────────────────── */

/*
 * Run on_pre_move hooks for ALL features in the grid.
 * Any feature may respond (e.g., Medusa checks LOS from its wall).
 * Returns the most severe result (KILL > SLIDE > OK).
 */
static PreMoveResult run_pre_move_hooks(const Grid* grid,
                                         int from_col, int from_row,
                                         int to_col, int to_row,
                                         Direction dir) {
    PreMoveResult result = PREMOVE_OK;

    for (int i = 0; i < grid->feature_count; i++) {
        const Feature* f = grid->features[i];
        if (f->vt->on_pre_move) {
            PreMoveResult r = f->vt->on_pre_move(f, grid,
                                                   from_col, from_row,
                                                   to_col, to_row, dir);
            if (r == PREMOVE_KILL) return PREMOVE_KILL;
            if (r == PREMOVE_SLIDE) result = PREMOVE_SLIDE;
        }
    }

    return result;
}

/* ── Push check ────────────────────────────────────────── */

/*
 * When Theseus's move is blocked, check if any feature in the grid
 * has an on_push hook that consumes the action.
 *
 * We check ALL features (not just those at source/target) because some
 * features (e.g. manual turnstile) may respond to pushes from tiles
 * other than the one the feature is placed on.
 */
static bool try_push(Grid* grid, int from_col, int from_row, Direction dir) {
    for (int i = 0; i < grid->feature_count; i++) {
        Feature* f = grid->features[i];
        if (f->vt->on_push &&
            f->vt->on_push(f, grid, from_col, from_row, dir)) {
            return true;
        }
    }

    return false;
}

/* ── Ice slide ─────────────────────────────────────────── */

/*
 * Check if a tile has an ice feature.
 */
static bool tile_has_ice(const Grid* grid, int col, int row) {
    const Cell* cell = grid_cell_const(grid, col, row);
    if (!cell) return false;
    for (int i = 0; i < cell->feature_count; i++) {
        if (strcmp(cell->features[i]->vt->name, "ice_tile") == 0) {
            return true;
        }
    }
    return false;
}

/*
 * Slide Theseus across ice tiles in the given direction.
 * Called after Theseus has already moved onto an ice tile.
 */
static TurnResult ice_slide(Grid* grid, Direction dir) {
    while (tile_has_ice(grid, grid->theseus_col, grid->theseus_row)) {
        if (!grid_can_move(grid, ENTITY_THESEUS,
                           grid->theseus_col, grid->theseus_row, dir)) {
            break;
        }

        grid_move_entity(grid, ENTITY_THESEUS, dir);

        if (grid_entities_collide(grid)) {
            grid->level_lost = true;
            return TURN_RESULT_LOSS_COLLISION;
        }

        if (grid_theseus_on_hazard(grid)) {
            grid->level_lost = true;
            return TURN_RESULT_LOSS_HAZARD;
        }
    }

    return TURN_RESULT_CONTINUE;
}

/* ── Exit check ────────────────────────────────────────── */

static bool try_exit(Grid* grid, Direction player_dir) {
    if (grid->theseus_col == grid->exit_col &&
        grid->theseus_row == grid->exit_row &&
        player_dir == grid->exit_side) {
        grid->level_won = true;
        return true;
    }
    return false;
}

/* ── Main turn resolution ──────────────────────────────── */

TurnResult turn_resolve(Grid* grid, Direction player_dir) {
    /* ── Phase 1: Theseus ── */

    if (player_dir != DIR_NONE) {
        /* Check for exit first */
        if (try_exit(grid, player_dir)) {
            grid->turn_count++;
            return TURN_RESULT_WIN;
        }

        int tc = grid->theseus_col + direction_dcol(player_dir);
        int tr = grid->theseus_row + direction_drow(player_dir);

        /* Run on_pre_move hooks before attempting the move */
        PreMoveResult pre = run_pre_move_hooks(grid,
                                                grid->theseus_col, grid->theseus_row,
                                                tc, tr, player_dir);
        if (pre == PREMOVE_KILL) {
            grid->level_lost = true;
            grid->turn_count++;
            return TURN_RESULT_LOSS_HAZARD;
        }

        /* Try to move */
        if (!grid_move_entity(grid, ENTITY_THESEUS, player_dir)) {
            /* Move blocked — try on_push hooks */
            if (try_push(grid, grid->theseus_col, grid->theseus_row, player_dir)) {
                /* Push consumed the action — Theseus doesn't move but turn proceeds */
            } else {
                return TURN_RESULT_BLOCKED;
            }
        } else {
            /* Move succeeded */
            if (pre == PREMOVE_SLIDE) {
                TurnResult slide_result = ice_slide(grid, player_dir);
                if (slide_result != TURN_RESULT_CONTINUE) {
                    grid->turn_count++;
                    return slide_result;
                }
            }

            if (grid_entities_collide(grid)) {
                grid->level_lost = true;
                grid->turn_count++;
                return TURN_RESULT_LOSS_COLLISION;
            }

            if (grid_theseus_on_hazard(grid)) {
                grid->level_lost = true;
                grid->turn_count++;
                return TURN_RESULT_LOSS_HAZARD;
            }
        }
    }

    /* ── Phase 2: Environment ── */

    turn_run_environment_phase(grid);

    if (grid_theseus_on_hazard(grid)) {
        grid->level_lost = true;
        grid->turn_count++;
        return TURN_RESULT_LOSS_HAZARD;
    }

    if (grid_entities_collide(grid)) {
        grid->level_lost = true;
        grid->turn_count++;
        return TURN_RESULT_LOSS_COLLISION;
    }

    /* ── Phase 3: Minotaur ── */

    minotaur_take_turn(grid);

    if (grid->level_lost) {
        grid->turn_count++;
        return TURN_RESULT_LOSS_COLLISION;
    }

    grid->turn_count++;
    return TURN_RESULT_CONTINUE;
}
