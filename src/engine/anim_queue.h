#ifndef ENGINE_ANIM_QUEUE_H
#define ENGINE_ANIM_QUEUE_H

#include "engine/tween.h"
#include "game/turn.h"
#include "game/anim_event.h"
#include <stdbool.h>

/*
 * AnimQueue — turn animation sequencer.
 *
 * Game logic resolves instantly (all phases computed immediately).
 * The anim queue then plays back the visual sequence in order:
 *   1. Theseus move animation (hop / ice-slide / teleport / push)
 *   2. On-leave effects (crumble, gate lock, plate toggle)
 *   3. Environment phase (sequential per-event animations)
 *   4. Minotaur step 1 animation
 *   5. Minotaur step 2 animation (if applicable)
 *
 * For undo, the queue can play in REVERSE order at 2× speed:
 *   1. Minotaur step 2 (reversed)
 *   2. Minotaur step 1 (reversed)
 *   3. Environment phase (events in reverse order, swapped from/to)
 *   4. On-leave effects (reversed)
 *   5. Theseus move (reversed)
 *
 * The input buffer window opens during the Minotaur's LAST step
 * animation (see input_buffer.h).
 */

typedef enum {
    ANIM_PHASE_IDLE,                  /* not playing, waiting for input */
    ANIM_PHASE_THESEUS,              /* Theseus move animation */
    ANIM_PHASE_THESEUS_EFFECTS,      /* on-leave effects (crumble, gate, plate) */
    ANIM_PHASE_ENVIRONMENT,          /* environment phase per-event animations */
    ANIM_PHASE_MINOTAUR_STEP1,       /* Minotaur step 1 */
    ANIM_PHASE_MINOTAUR_STEP2,       /* Minotaur step 2 */
} AnimPhase;

/* Theseus sub-phase for multi-part moves */
typedef enum {
    THESEUS_SUB_HOP,          /* normal hop or first hop of ice slide */
    THESEUS_SUB_ICE_SLIDE,    /* constant-velocity slide across ice waypoints */
    THESEUS_SUB_TELEPORT_OUT, /* shrink/fade at source */
    THESEUS_SUB_TELEPORT_IN,  /* grow/fade at destination */
    THESEUS_SUB_PUSH,         /* push move (box slides, Theseus steps) */
} TheseusSubPhase;

/* Animation timing constants (seconds) */
#define ANIM_THESEUS_DURATION    0.15f
#define ANIM_ENVIRONMENT_DURATION 0.10f
#define ANIM_MINOTAUR_DURATION   0.15f
#define ANIM_HOP_HEIGHT          0.3f
#define ANIM_REVERSE_SPEED       2.0f   /* speed multiplier for reverse (undo) playback */

/* Per-event-type durations */
#define ANIM_ICE_SLIDE_PER_TILE  0.06f
#define ANIM_TELEPORT_HALF       0.10f
#define ANIM_PUSH_DURATION       0.15f
#define ANIM_CRUMBLE_DURATION    0.15f
#define ANIM_GATE_LOCK_DURATION  0.12f
#define ANIM_PLATE_DURATION      0.10f
#define ANIM_SPIKE_DURATION      0.12f
#define ANIM_AUTO_TURNSTILE_DURATION 0.25f
#define ANIM_PLATFORM_DURATION   0.20f
#define ANIM_CONVEYOR_DURATION   0.15f
#define ANIM_TURNSTILE_DURATION  0.20f
#define ANIM_ENV_MIN_PAUSE       0.10f

typedef struct {
    AnimPhase        phase;
    TurnRecord       record;
    bool             playing;
    bool             reversing;       /* true during reverse (undo) playback */

    /* ── Theseus phase ─────────────────────────── */
    AnimEventType    theseus_event_type;   /* what kind of move */
    TheseusSubPhase  theseus_sub;
    Tween            move_x;              /* grid-space X (column) */
    Tween            move_y;              /* grid-space Y (row) */
    Tween            hop;                 /* parabolic arc height */

    /* Ice slide: waypoint tracking */
    int              ice_wp_index;        /* current waypoint being slid to */
    int              ice_wp_count;        /* total waypoints */
    int              ice_wp_cols[ICE_SLIDE_MAX_WAYPOINTS];
    int              ice_wp_rows[ICE_SLIDE_MAX_WAYPOINTS];

    /* Teleport effect progress (0→1 for each half) */
    Tween            effect;

    /* Push: secondary object (box) tweens */
    Tween            aux_x;
    Tween            aux_y;

    /* Turnstile rotation progress */
    Tween            rotation;

    /* ── Theseus effects / Environment phase ──── */
    int              effect_event_idx;    /* index into record.events for current effect/env event */
    Tween            phase_tween;         /* generic progress tween for current event */

    /* Environment phase actor position tracking.
     * Updated progressively as env events complete, so actors don't
     * snap back to pre-environment positions between events. */
    float            env_theseus_col, env_theseus_row;
    float            env_minotaur_col, env_minotaur_row;

    /* ── Minotaur phase ────────────────────────── */
    Tween            mino_x;
    Tween            mino_y;
} AnimQueue;

/* Initialize the animation queue (idle state). */
void anim_queue_init(AnimQueue* aq);

/*
 * Start playing a turn's animation.
 * The TurnRecord must already be filled in by turn_resolve().
 */
void anim_queue_start(AnimQueue* aq, const TurnRecord* record);

/*
 * Start playing a turn's animation in REVERSE (for undo).
 * Plays phases in reverse order (Mino2→Mino1→Env→Effects→Theseus)
 * at 2× speed with swapped from/to positions.
 *
 * The grid should NOT be restored until this animation completes
 * (the caller defers undo_pop until anim finishes).
 */
void anim_queue_start_reverse(AnimQueue* aq, const TurnRecord* record);

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

/* ── Query functions for per-event rendering ─────── */

/* What type of Theseus move is currently animating? */
AnimEventType anim_queue_theseus_event_type(const AnimQueue* aq);

/* Teleport effect progress: 0→1 for fade-out, 0→1 for fade-in.
 * Returns -1 if not in teleport animation.
 * out_phase: 0 = fading out, 1 = fading in */
float anim_queue_teleport_progress(const AnimQueue* aq, int* out_phase);

/* Secondary object position (box during push, platform during move) */
void anim_queue_aux_pos(const AnimQueue* aq,
                        float* out_col, float* out_row);

/* Rotation angle progress (0→1 for 90° rotation) */
float anim_queue_rotation_progress(const AnimQueue* aq);

/* Environment/effect event progress (0→1 for current event) */
float anim_queue_effect_progress(const AnimQueue* aq);

/* Current environment/effect event being animated (or NULL) */
const AnimEvent* anim_queue_current_event(const AnimQueue* aq);

/* Is the Theseus phase in ice-slide sub-phase (no hop)? */
bool anim_queue_is_ice_sliding(const AnimQueue* aq);

/* Is the queue currently playing in reverse (undo)? */
bool anim_queue_is_reversing(const AnimQueue* aq);

#endif /* ENGINE_ANIM_QUEUE_H */
