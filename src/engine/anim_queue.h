#ifndef ENGINE_ANIM_QUEUE_H
#define ENGINE_ANIM_QUEUE_H

#include "engine/tween.h"
#include "game/turn.h"
#include <stdbool.h>

/*
 * AnimQueue — turn animation sequencer.
 *
 * Game logic resolves instantly (all phases computed immediately).
 * The anim queue then plays back the visual sequence in order:
 *   1. Theseus move animation
 *   2. Environment phase (brief pause for visual updates)
 *   3. Minotaur step 1 animation
 *   4. Minotaur step 2 animation (if applicable)
 *
 * Animations are never fast-forwarded or skipped.
 *
 * The input buffer window opens during the Minotaur's LAST step
 * animation (see input_buffer.h).
 */

typedef enum {
    ANIM_PHASE_IDLE,              /* not playing, waiting for input */
    ANIM_PHASE_THESEUS,           /* Theseus move animation */
    ANIM_PHASE_ENVIRONMENT,       /* environment phase visual pause */
    ANIM_PHASE_MINOTAUR_STEP1,    /* Minotaur step 1 */
    ANIM_PHASE_MINOTAUR_STEP2,    /* Minotaur step 2 */
} AnimPhase;

/* Animation timing constants (seconds) */
#define ANIM_THESEUS_DURATION    0.15f
#define ANIM_ENVIRONMENT_DURATION 0.10f
#define ANIM_MINOTAUR_DURATION   0.15f
/* Hop height as fraction of tile size */
#define ANIM_HOP_HEIGHT          0.3f

typedef struct {
    AnimPhase   phase;
    TurnRecord  record;
    bool        playing;

    /* Current phase tween (position interpolation) */
    Tween       move_x;       /* grid-space X (column) */
    Tween       move_y;       /* grid-space Y (row) */
    Tween       hop;          /* parabolic arc height (for Theseus) */

    /* Minotaur position tweens */
    Tween       mino_x;
    Tween       mino_y;
} AnimQueue;

/* Initialize the animation queue (idle state). */
void anim_queue_init(AnimQueue* aq);

/*
 * Start playing a turn's animation.
 * The TurnRecord must already be filled in by turn_resolve().
 */
void anim_queue_start(AnimQueue* aq, const TurnRecord* record);

/*
 * Advance the animation by dt seconds.
 * When a phase completes, automatically transitions to the next.
 * Returns to IDLE when all phases are done.
 */
void anim_queue_update(AnimQueue* aq, float dt);

/* Is the queue currently playing an animation? */
bool anim_queue_is_playing(const AnimQueue* aq);

/* Get the current animation phase. */
AnimPhase anim_queue_phase(const AnimQueue* aq);

/*
 * Is the input buffer window currently open?
 * True during the Minotaur's LAST step animation.
 */
bool anim_queue_in_buffer_window(const AnimQueue* aq);

/*
 * Get interpolated Theseus position in grid space (float col, row).
 * Valid during THESEUS phase; otherwise returns final position.
 */
void anim_queue_theseus_pos(const AnimQueue* aq,
                            float* out_col, float* out_row,
                            float* out_hop);

/*
 * Get interpolated Minotaur position in grid space (float col, row).
 * Valid during MINOTAUR phases; otherwise returns final position.
 */
void anim_queue_minotaur_pos(const AnimQueue* aq,
                             float* out_col, float* out_row);

#endif /* ENGINE_ANIM_QUEUE_H */
