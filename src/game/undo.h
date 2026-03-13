#ifndef GAME_UNDO_H
#define GAME_UNDO_H

#include "grid.h"
#include <stdbool.h>

/*
 * Undo system — full turn-granularity snapshots.
 *
 * Each snapshot captures:
 *   - Entity positions (Theseus, Minotaur)
 *   - Turn count
 *   - All mutable feature state (via feature vtable snapshot hooks)
 *
 * The undo stack is unlimited depth (dynamically allocated).
 * Per the design doc, undo reverts the entire last turn atomically.
 *
 * Usage:
 *   1. Before resolving a turn: undo_push(undo, grid)
 *   2. To undo:                 undo_pop(undo, grid)
 *   3. To reset to initial:     undo_reset(undo, grid) — pops all
 */

/* Maximum snapshots (generous — most puzzles < 200 turns) */
#define UNDO_MAX_DEPTH  512

/*
 * Compact per-cell state for undo (walls + impassable only).
 * Feature pointer linkage is rebuilt from feature col/row on restore.
 */
typedef struct {
    bool walls[DIR_COUNT];
    bool impassable;
} CellSnapshot;

typedef struct {
    int theseus_col, theseus_row;
    int minotaur_col, minotaur_row;
    int turn_count;
    bool level_won;
    bool level_lost;

    /*
     * Feature snapshot blob.  Contains serialized state of all features
     * that have snapshot_size > 0, packed sequentially.
     * Layout: [feature_0 state][feature_1 state]...
     */
    void*  feature_blob;
    size_t feature_blob_size;

    /*
     * Cell state snapshot (walls + impassable for each cell).
     * Required for features that modify grid structure (pressure plates,
     * locking gates, crumbling floors, turnstiles).
     */
    CellSnapshot* cell_snapshots;
    int cell_count;

    /*
     * Feature position snapshot.  Stores col/row for each feature
     * (packed as [col0, row0, col1, row1, ...]).
     * Required for features that move (moving platform, auto-turnstile).
     */
    int* feature_positions;
    int feature_pos_count;   /* number of features (array has 2x this) */
} UndoSnapshot;

typedef struct {
    UndoSnapshot snapshots[UNDO_MAX_DEPTH];
    int count;

    /*
     * The "initial" snapshot — for full reset to level start.
     * Stored separately so it's never popped.
     */
    UndoSnapshot initial;
    bool has_initial;
} UndoStack;

/* ── Lifecycle ─────────────────────────────────────────── */

void undo_init(UndoStack* stack);
void undo_clear(UndoStack* stack);

/* ── Operations ────────────────────────────────────────── */

/*
 * Save the initial state (call once after level load).
 * This is the state undo_reset() restores to.
 */
void undo_save_initial(UndoStack* stack, const Grid* grid);

/*
 * Push current state onto the undo stack.
 * Call BEFORE resolving each turn.
 */
void undo_push(UndoStack* stack, const Grid* grid);

/*
 * Pop the most recent snapshot and restore the grid to that state.
 * Returns true if an undo was performed, false if stack is empty.
 */
bool undo_pop(UndoStack* stack, Grid* grid);

/*
 * Reset the grid to its initial state (level start).
 * Clears the undo stack. Returns true if reset was performed.
 */
bool undo_reset(UndoStack* stack, Grid* grid);

/*
 * Query how many undo steps are available.
 */
int undo_depth(const UndoStack* stack);

#endif /* GAME_UNDO_H */
