#ifndef GAME_FEATURE_H
#define GAME_FEATURE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Feature — pluggable environmental behaviour on the grid.
 *
 * Each environmental element (spike traps, pressure plates, locking gates,
 * turnstiles, teleporters, crumbling floors, medusa walls, ice tiles,
 * moving platforms, groove boxes, etc.) implements this vtable.  The core
 * turn loop never changes; new mechanics are added by writing a new .c
 * file that fills in the relevant hooks and registering it with the grid.
 *
 * NULL function pointers are safe — the core skips them.
 */

/* Forward declarations */
typedef struct Grid   Grid;
typedef struct Feature Feature;

/* Which entity is interacting */
typedef enum {
    ENTITY_THESEUS,
    ENTITY_MINOTAUR
} EntityID;

/* Cardinal directions */
typedef enum {
    DIR_NORTH = 0,
    DIR_SOUTH,
    DIR_EAST,
    DIR_WEST,
    DIR_COUNT,
    DIR_NONE = -1
} Direction;

/* Opposite direction helper */
static inline Direction direction_opposite(Direction d) {
    switch (d) {
        case DIR_NORTH: return DIR_SOUTH;
        case DIR_SOUTH: return DIR_NORTH;
        case DIR_EAST:  return DIR_WEST;
        case DIR_WEST:  return DIR_EAST;
        default:        return DIR_NONE;
    }
}

/* Direction delta helpers (design: cols W→E, rows S→N) */
static inline int direction_dcol(Direction d) {
    return (d == DIR_EAST) ? 1 : (d == DIR_WEST) ? -1 : 0;
}
static inline int direction_drow(Direction d) {
    return (d == DIR_NORTH) ? 1 : (d == DIR_SOUTH) ? -1 : 0;
}

/*
 * Result of on_pre_move hook.
 */
typedef enum {
    PREMOVE_OK,     /* move proceeds normally */
    PREMOVE_KILL,   /* Theseus dies (e.g. Medusa gaze) */
    PREMOVE_SLIDE   /* Theseus slides in this direction until stopped (ice) */
} PreMoveResult;

/*
 * Feature vtable — only implement hooks your feature needs.
 * Leave the rest as NULL (or zero-init the struct).
 */
typedef struct FeatureVTable {
    const char* name;   /* human-readable, e.g. "spike_trap" */

    /*
     * blocks_movement
     * Can 'who' move from (from_col,from_row) toward (to_col,to_row)?
     * Return true to BLOCK the move.
     * Called for Theseus, Minotaur, and platform pushes alike.
     *
     * NOTE: For features on the SOURCE cell, this can also block leaving.
     * The turn resolver checks features on both source and target cells.
     */
    bool (*blocks_movement)(const Feature* self, const Grid* grid,
                            EntityID who,
                            int from_col, int from_row,
                            int to_col,   int to_row);

    /*
     * on_pre_move
     * Called BEFORE Theseus commits a move, while still on (from_col/row)
     * and intending to move toward (to_col/to_row).
     * Return PREMOVE_KILL to kill Theseus (e.g. Medusa line-of-sight).
     * Return PREMOVE_SLIDE to indicate the move should slide (ice tiles).
     * Return PREMOVE_OK to allow the move normally.
     *
     * Only called for ENTITY_THESEUS. The Minotaur never triggers pre-move.
     * Features on ANY cell in the grid may respond (e.g. Medusa checks
     * line of sight from its wall position).
     */
    PreMoveResult (*on_pre_move)(const Feature* self, const Grid* grid,
                                 int from_col, int from_row,
                                 int to_col,   int to_row,
                                 Direction dir);

    /*
     * on_enter
     * Called after 'who' successfully moves INTO this feature's tile.
     * Use for teleporters, pressure plates activating, etc.
     */
    void (*on_enter)(Feature* self, Grid* grid, EntityID who,
                     int col, int row);

    /*
     * on_leave
     * Called after 'who' moves OUT of this feature's tile.
     * Use for spike trap arming, crumbling floor marking, etc.
     */
    void (*on_leave)(Feature* self, Grid* grid, EntityID who,
                     int col, int row);

    /*
     * on_push
     * Called when Theseus tries to move into a tile/wall that this feature
     * occupies or is attached to, and the move would normally be blocked.
     * Return true if the push was consumed (Theseus's action is spent but
     * he does not move — e.g. manual turnstile rotates walls).
     * Return false if the push had no effect (normal block).
     *
     * For groove boxes: the push moves the box and Theseus follows, so
     * return true and update positions internally.
     *
     * Only called for ENTITY_THESEUS. The Minotaur cannot push.
     */
    bool (*on_push)(Feature* self, Grid* grid,
                    int from_col, int from_row, Direction dir);

    /*
     * on_environment_phase
     * Runs during the environment phase (between Theseus and Minotaur).
     * Use for spike trap activation, auto-turnstile rotation, moving
     * platform advancement, crumbling floor collapse, etc.
     */
    void (*on_environment_phase)(Feature* self, Grid* grid);

    /*
     * is_hazardous
     * Return true if the tile is currently deadly to Theseus.
     * The Minotaur is immune to all hazards (per design doc).
     * Checked after every entity step and after environment phase.
     */
    bool (*is_hazardous)(const Feature* self, const Grid* grid,
                         int col, int row);

    /*
     * snapshot_size / snapshot_save / snapshot_restore
     * Support for the undo system.  Features with mutable state
     * (e.g. spike trap phase counter) must implement these so
     * that undo can restore their state.
     *
     * Features with no mutable state can leave these NULL
     * (snapshot_size returning 0 means the feature is skipped).
     */
    size_t (*snapshot_size)(const Feature* self);
    void   (*snapshot_save)(const Feature* self, void* buf);
    void   (*snapshot_restore)(Feature* self, const void* buf);

    /*
     * destroy
     * Free any memory owned by this feature instance.
     */
    void (*destroy)(Feature* self);
} FeatureVTable;

/*
 * Feature instance — one per placed feature on the grid.
 * Concrete feature types embed their own data after this struct
 * (or allocate separately and store a pointer in `data`).
 */
struct Feature {
    const FeatureVTable* vt;
    int col, row;       /* grid position */
    void* data;         /* per-instance data (owned by feature) */
};

/*
 * Feature constructor helper.
 * Allocates a Feature, sets vtable and position, zeroes data.
 */
Feature* feature_create(const FeatureVTable* vt, int col, int row);

/*
 * Destroy a feature (calls vt->destroy if set, then frees).
 */
void feature_free(Feature* f);

#endif /* GAME_FEATURE_H */
