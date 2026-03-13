#ifndef GAME_TURN_H
#define GAME_TURN_H

#include "grid.h"
#include "feature.h"
#include "anim_event.h"

/*
 * Turn resolution — the fixed game loop that never changes.
 *
 * Turn sequence:
 *   1. Theseus Phase   — player action (move or wait)
 *   2. Environment Phase — all features run on_environment_phase
 *   3. Minotaur Phase  — Minotaur takes 2 greedy steps
 *
 * Win/loss checks happen:
 *   - After Theseus moves (collision with Minotaur = loss,
 *     stepping through exit = win)
 *   - After environment phase (hazard kills Theseus = loss)
 *   - After each Minotaur step (collision = loss)
 *
 * Undo snapshots are pushed BEFORE the turn executes, so the
 * player can revert the entire turn atomically.
 */

/* Result of a turn */
typedef enum {
    TURN_RESULT_CONTINUE,       /* turn resolved, game continues */
    TURN_RESULT_WIN,            /* Theseus reached the exit */
    TURN_RESULT_LOSS_COLLISION, /* Theseus and Minotaur on same tile */
    TURN_RESULT_LOSS_HAZARD,    /* Theseus killed by environment */
    TURN_RESULT_BLOCKED         /* Theseus tried to move but was blocked */
} TurnResult;

/*
 * TurnRecord — captures intermediate positions for animation playback.
 *
 * Game logic resolves instantly; the renderer plays back the visual
 * sequence using the positions recorded here.
 *
 * The events[] array captures per-feature animation events (ice slide
 * waypoints, teleport, push effects, environment effects, etc.) for
 * rich visual playback beyond simple position interpolation.
 */
typedef struct TurnRecord {
    /* Theseus phase */
    int  theseus_from_col, theseus_from_row;
    int  theseus_to_col,   theseus_to_row;
    bool theseus_moved;         /* true if Theseus changed position */
    bool theseus_pushed;        /* true if Theseus pushed something (didn't move but turn proceeds) */

    /* Minotaur phase */
    int  minotaur_start_col,  minotaur_start_row;
    int  minotaur_after1_col, minotaur_after1_row;  /* after step 1 */
    int  minotaur_after2_col, minotaur_after2_row;  /* after step 2 */
    int  minotaur_steps;                             /* 0, 1, or 2 */

    /* Result */
    TurnResult result;

    /* Animation events — per-feature visual changes */
    AnimEvent events[ANIM_EVENT_MAX];
    int       event_count;
} TurnRecord;

/*
 * Push an animation event onto the record's event list.
 * Safe to call with NULL record (no-op). Silently drops events
 * if the array is full (ANIM_EVENT_MAX).
 */
static inline void turn_record_push_event(TurnRecord* record,
                                           const AnimEvent* evt) {
    if (!record) return;
    if (record->event_count >= ANIM_EVENT_MAX) return;
    record->events[record->event_count++] = *evt;
}

/*
 * Execute a full turn given the player's chosen direction.
 *
 * dir = DIR_NONE means "wait" (Theseus stays, env + Minotaur still act).
 *
 * The caller must push an undo snapshot BEFORE calling this if undo
 * support is desired.
 *
 * If record is non-NULL, intermediate positions are captured for
 * animation playback.
 *
 * Returns the result of the turn.
 */
TurnResult turn_resolve(Grid* grid, Direction player_dir, TurnRecord* record);

/*
 * Run only the environment phase.
 * Calls on_environment_phase on every feature in the grid.
 * Useful for testing features in isolation.
 */
void turn_run_environment_phase(Grid* grid);

#endif /* GAME_TURN_H */
