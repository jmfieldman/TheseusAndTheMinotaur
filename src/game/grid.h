#ifndef GAME_GRID_H
#define GAME_GRID_H

#include <stdbool.h>
#include "feature.h"

/*
 * Grid — the logical game board.
 *
 * Coordinate convention (from design doc):
 *   Columns increase West → East   (col 0 = leftmost)
 *   Rows    increase South → North (row 0 = bottom / camera-facing)
 *
 * Walls live on cell edges.  Each cell stores walls on its four sides.
 * Shared edges are stored on both cells to keep queries O(1).
 *
 * Features are stored in a flat array on the Grid AND linked per-cell
 * for fast spatial queries.
 */

/* Maximum features per cell (multiple features can stack) */
#define MAX_FEATURES_PER_CELL  8
/* Maximum total features per grid */
#define MAX_FEATURES           128

typedef struct {
    bool walls[DIR_COUNT];              /* N, S, E, W edge walls */
    bool impassable;                    /* neither actor can enter */
    Feature* features[MAX_FEATURES_PER_CELL];
    int feature_count;
} Cell;

typedef struct Grid {
    int cols, rows;
    Cell* cells;                        /* cols * rows, row-major: cells[row * cols + col] */

    /* Entity positions */
    int theseus_col,  theseus_row;
    int minotaur_col, minotaur_row;

    /* Entrance / exit doors */
    int entrance_col, entrance_row;
    Direction entrance_side;            /* which boundary wall the door is on */
    int exit_col,     exit_row;
    Direction exit_side;

    /* All features (owned by grid, destroyed on grid_destroy) */
    Feature* features[MAX_FEATURES];
    int feature_count;

    /* Level metadata */
    char level_id[64];
    char level_name[64];
    char biome[64];
    int  optimal_turns;
    int  turn_count;                    /* current turn number */
    bool level_won;
    bool level_lost;
} Grid;

/* ── Lifecycle ─────────────────────────────────────────── */

/*
 * Allocate a grid of the given dimensions.  All cells start with
 * no walls, passable, and no features.  Boundary walls are added
 * automatically (grid edges are always walled).
 */
Grid* grid_create(int cols, int rows);

/*
 * Deep-destroy: frees all features, cells, and the grid itself.
 */
void grid_destroy(Grid* grid);

/* ── Cell access ───────────────────────────────────────── */

/* Returns NULL if out of bounds. */
Cell* grid_cell(Grid* grid, int col, int row);
const Cell* grid_cell_const(const Grid* grid, int col, int row);

/* Bounds check */
bool grid_in_bounds(const Grid* grid, int col, int row);

/* ── Walls ─────────────────────────────────────────────── */

/*
 * Set a wall on one side of a cell.
 * Automatically mirrors the wall on the neighbouring cell's opposite side.
 */
void grid_set_wall(Grid* grid, int col, int row, Direction side, bool present);

/*
 * Query whether a wall exists on the given side of (col, row).
 * Out-of-bounds sides are always walled.
 */
bool grid_has_wall(const Grid* grid, int col, int row, Direction side);

/* ── Movement queries ──────────────────────────────────── */

/*
 * Can 'who' move from (col,row) in the given direction?
 * Checks: bounds, walls, impassable, and all feature blocks_movement hooks.
 * Does NOT check entity collisions (Theseus walking onto Minotaur is
 * a loss condition, not a movement block).
 */
bool grid_can_move(const Grid* grid, EntityID who,
                   int col, int row, Direction dir);

/*
 * Attempt to move 'who' one step in 'dir'.
 * Returns true if the move was executed (position updated, on_leave/on_enter fired).
 * Returns false if blocked.
 */
bool grid_move_entity(Grid* grid, EntityID who, Direction dir);

/* ── Feature management ────────────────────────────────── */

/*
 * Add a feature to the grid at (col, row).
 * The grid takes ownership of the feature pointer.
 * Returns false if limits are exceeded.
 */
bool grid_add_feature(Grid* grid, Feature* feature);

/*
 * Get all features at a specific cell.
 * Returns count written to out_features (up to max_out).
 */
int grid_get_features_at(const Grid* grid, int col, int row,
                         Feature** out_features, int max_out);

/* ── Entity position helpers ───────────────────────────── */

void grid_get_entity_pos(const Grid* grid, EntityID who, int* out_col, int* out_row);
void grid_set_entity_pos(Grid* grid, EntityID who, int col, int row);

/* ── Exit / win helpers ────────────────────────────────── */

/*
 * Check if Theseus is on the exit tile and facing the exit door.
 * The actual "stepping through" is handled by the turn resolver.
 */
bool grid_theseus_at_exit(const Grid* grid);

/*
 * Check if Theseus and Minotaur share a tile (loss condition).
 */
bool grid_entities_collide(const Grid* grid);

/*
 * Check if any hazardous feature exists at Theseus's position.
 */
bool grid_theseus_on_hazard(const Grid* grid);

#endif /* GAME_GRID_H */
