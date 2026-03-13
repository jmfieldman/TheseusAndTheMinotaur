#include "turn.h"
#include "minotaur.h"
#include "../engine/utils.h"

/* ── Environment phase ─────────────────────────────────── */

void turn_run_environment_phase(Grid* grid) {
    for (int i = 0; i < grid->feature_count; i++) {
        Feature* f = grid->features[i];
        if (f->vt->on_environment_phase) {
            f->vt->on_environment_phase(f, grid);
        }
    }
}

/* ── Exit check ────────────────────────────────────────── */

/*
 * Check if Theseus is trying to step through the exit door.
 * The exit door is on a boundary wall; moving in that direction
 * takes Theseus off the grid (instant win).
 *
 * We need to check this BEFORE the normal wall check, because
 * the exit wall was removed during level loading to allow passage.
 */
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

        /* Try to move */
        if (!grid_move_entity(grid, ENTITY_THESEUS, player_dir)) {
            return TURN_RESULT_BLOCKED;
        }

        /* Check: did Theseus walk onto the Minotaur? */
        if (grid_entities_collide(grid)) {
            grid->level_lost = true;
            grid->turn_count++;
            return TURN_RESULT_LOSS_COLLISION;
        }
    }

    /* ── Phase 2: Environment ── */

    turn_run_environment_phase(grid);

    /* Check: did a hazard kill Theseus? */
    if (grid_theseus_on_hazard(grid)) {
        grid->level_lost = true;
        grid->turn_count++;
        return TURN_RESULT_LOSS_HAZARD;
    }

    /* ── Phase 3: Minotaur ── */

    minotaur_take_turn(grid);

    if (grid->level_lost) {
        grid->turn_count++;
        return TURN_RESULT_LOSS_COLLISION;
    }

    /* Turn completed successfully */
    grid->turn_count++;
    return TURN_RESULT_CONTINUE;
}
