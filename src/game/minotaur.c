#include "minotaur.h"
#include "../engine/utils.h"

/*
 * Determine which direction closes horizontal distance to Theseus.
 * Returns DIR_NONE if same column.
 */
static Direction horizontal_toward_theseus(const Grid* grid) {
    if (grid->minotaur_col < grid->theseus_col) return DIR_EAST;
    if (grid->minotaur_col > grid->theseus_col) return DIR_WEST;
    return DIR_NONE;
}

/*
 * Determine which direction closes vertical distance to Theseus.
 * Returns DIR_NONE if same row.
 */
static Direction vertical_toward_theseus(const Grid* grid) {
    if (grid->minotaur_row < grid->theseus_row) return DIR_NORTH;
    if (grid->minotaur_row > grid->theseus_row) return DIR_SOUTH;
    return DIR_NONE;
}

/*
 * Check if a move would take the Minotaur through the exit door.
 * The Minotaur is not allowed to pass through the exit.
 */
static bool is_exit_move(const Grid* grid, int from_col, int from_row,
                         Direction dir) {
    /* The exit is on a boundary wall.  If the Minotaur is at the exit cell
     * and trying to move in the exit direction, that would take it through
     * the exit door — block it. */
    if (from_col == grid->exit_col && from_row == grid->exit_row &&
        dir == grid->exit_side) {
        return true;
    }
    return false;
}

bool minotaur_step(Grid* grid) {
    Direction h_dir = horizontal_toward_theseus(grid);
    Direction v_dir = vertical_toward_theseus(grid);

    /* Try horizontal first (priority) */
    if (h_dir != DIR_NONE) {
        if (!is_exit_move(grid, grid->minotaur_col, grid->minotaur_row, h_dir) &&
            grid_can_move(grid, ENTITY_MINOTAUR,
                          grid->minotaur_col, grid->minotaur_row, h_dir)) {
            grid_move_entity(grid, ENTITY_MINOTAUR, h_dir);
            return true;
        }
    }

    /* Try vertical */
    if (v_dir != DIR_NONE) {
        if (!is_exit_move(grid, grid->minotaur_col, grid->minotaur_row, v_dir) &&
            grid_can_move(grid, ENTITY_MINOTAUR,
                          grid->minotaur_col, grid->minotaur_row, v_dir)) {
            grid_move_entity(grid, ENTITY_MINOTAUR, v_dir);
            return true;
        }
    }

    /* Both blocked — forfeit step */
    return false;
}

int minotaur_take_turn(Grid* grid) {
    int steps = 0;

    for (int i = 0; i < 2; i++) {
        if (minotaur_step(grid)) {
            steps++;
        }

        /* Check collision after each step */
        if (grid_entities_collide(grid)) {
            grid->level_lost = true;
            break;
        }
    }

    return steps;
}
