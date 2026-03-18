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
 * The input buffer window is open during ANY animation phase (forward
 * or reverse). When buffered input is pending, remaining animations
 * play at a user-configurable speed multiplier (see settings.h).
 */

typedef enum {
    ANIM_PHASE_IDLE,                  /* not playing, waiting for input */
    ANIM_PHASE_THESEUS,              /* Theseus move animation */
    ANIM_PHASE_THESEUS_EFFECTS,      /* on-leave effects (crumble, gate, plate) */
    ANIM_PHASE_ENVIRONMENT,          /* environment phase per-event animations */
    ANIM_PHASE_MINOTAUR_STEP1,       /* Minotaur step 1 */
    ANIM_PHASE_MINOTAUR_STEP2,       /* Minotaur step 2 */
    ANIM_PHASE_WIN_EXIT,             /* forced exit hop onto virtual tile */
    ANIM_PHASE_WIN_GATE,             /* exit door locks behind Theseus */
    ANIM_PHASE_WIN_CELEBRATE,        /* looping in-place celebration bounce */
} AnimPhase;

/* Theseus sub-phase for multi-part moves */
typedef enum {
    THESEUS_SUB_HOP,              /* normal hop or first hop of ice slide */
    THESEUS_SUB_ICE_SLIDE,        /* constant-velocity slide across ice waypoints */
    THESEUS_SUB_ICE_EXIT_HOP,     /* landing hop when sliding off ice onto normal tile */
    THESEUS_SUB_ICE_WALL_BUMP,    /* slide into wall and bounce back on ice */
    THESEUS_SUB_TELEPORT_OUT,     /* shrink/fade at source */
    THESEUS_SUB_TELEPORT_IN,      /* grow/fade at destination */
    THESEUS_SUB_PUSH,             /* push move (box slides, Theseus steps) */
} TheseusSubPhase;

/* Minotaur sub-phase for teleport moves */
typedef enum {
    MINO_SUB_ROLL,                /* normal roll/stomp to destination */
    MINO_SUB_TELEPORT_OUT,        /* beam-up at teleporter tile */
    MINO_SUB_TELEPORT_IN,         /* beam-down at destination */
} MinoSubPhase;

/* Animation timing constants (seconds) */
#define ANIM_THESEUS_DURATION    0.15f
#define ANIM_ENVIRONMENT_DURATION 0.10f
#define ANIM_MINOTAUR_DURATION   0.15f
#define ANIM_HOP_HEIGHT          0.3f
#define ANIM_REVERSE_SPEED       2.0f   /* speed multiplier for reverse (undo) playback */
/* Default fast-forward speed. Actual value comes from g_settings.anim_speed (1.0–4.0). */
#define ANIM_FAST_FORWARD_SPEED_DEFAULT  2.0f

/* Per-event-type durations */
#define ANIM_ICE_SLIDE_PER_TILE  0.06f
#define ANIM_ICE_EXIT_HOP_DUR   0.10f  /* short hop when sliding off ice */
#define ANIM_ICE_WALL_BUMP_DUR  0.40f  /* slide into wall + bounce back */
#define ANIM_TELEPORT_HALF       0.10f
#define ANIM_PUSH_DURATION       0.15f
#define ANIM_CRUMBLE_DURATION    0.15f
#define ANIM_GATE_LOCK_DURATION  0.12f
#define ANIM_PLATE_DURATION      0.10f
#define ANIM_SPIKE_DURATION      0.12f
#define ANIM_AUTO_TURNSTILE_DURATION 0.25f   /* longer for readable platform rotation */
#define ANIM_PLATFORM_DURATION   0.20f
#define ANIM_CONVEYOR_DURATION   0.15f
#define ANIM_TURNSTILE_DURATION  0.20f
#define ANIM_ENV_MIN_PAUSE       0.10f
#define ANIM_WIN_EXIT_DURATION   0.15f  /* forced exit hop (~same as normal hop) */
#define ANIM_WIN_GATE_DURATION   0.30f  /* exit door lock animation */
#define ANIM_WIN_CELEBRATE_HOP   0.70f  /* one celebration jump duration (seconds) */
#define ANIM_WIN_CELEBRATE_REST  0.35f  /* pause at rest between jumps (seconds) */
#define ANIM_WIN_CELEBRATE_HEIGHT 1.00f /* peak height of celebration hop */

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
    bool             ice_hit_wall;        /* true if slide ended by hitting wall */
    float            ice_bump_dir_x;      /* slide direction for wall bump */
    float            ice_bump_dir_z;

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
    int              mino_dir_col;    /* movement direction for current step (-1/0/+1) */
    int              mino_dir_row;    /* movement direction for current step (-1/0/+1) */

    /* Minotaur teleport sub-phases */
    MinoSubPhase     mino_sub;
    Tween            mino_effect;         /* beam progress 0→1 */
    bool             mino_teleporting;    /* is current step a teleport? */
    int              mino_tp_tile_col, mino_tp_tile_row;   /* teleporter tile pos */
    int              mino_tp_dest_col, mino_tp_dest_row;   /* teleport destination */

    /* ── Fast-forward ─────────────────────────── */
    bool             fast_forward;    /* true when buffered input is pending */

    /* ── Win exit animation ────────────────────── */
    Tween            win_exit_x;     /* hop X (column) */
    Tween            win_exit_y;     /* hop Y (row) */
    Tween            win_exit_hop;   /* parabolic arc height */
    Direction        win_exit_dir;   /* direction Theseus exits */

    /* ── Win gate lock animation ───────────────── */
    Tween            win_gate;       /* gate lock progress 0→1 */
    int              win_gate_col;   /* exit tile col (where gate appears) */
    int              win_gate_row;   /* exit tile row */
    Direction        win_gate_side;  /* which side of the tile the gate is on */

    /* ── Win celebrate loop ────────────────────── */
    float            win_celebrate_timer;  /* seconds into current jump (0→HOP_DUR) or rest (0→REST_DUR) */
    bool             win_celebrate_resting; /* true = in rest pause between jumps */
    bool             win_is_optimal;       /* true if turn_count <= optimal_turns */
    float            win_celebrate_col;    /* position (virtual tile) */
    float            win_celebrate_row;

    /* ── Walk-into death reverse override ──────── */
    bool             walk_into_reverse;       /* true: reverse hop uses fractional start pos */
    float            walk_into_start_col;     /* fractional start col for reverse hop */
    float            walk_into_start_row;     /* fractional start row for reverse hop */
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
 * True during ANY animation phase (forward or reverse).
 * The player can queue their next action at any time while
 * animations are playing, and fast-forward kicks in immediately.
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

/* Is the Theseus phase in ice wall bump sub-phase? */
bool anim_queue_is_ice_wall_bumping(const AnimQueue* aq);

/* Get ice wall bump progress (0→1) and direction.
 * Returns -1.0 if not in wall bump. */
float anim_queue_ice_bump_progress(const AnimQueue* aq,
                                    float* out_dir_x, float* out_dir_z);

/* Is the queue currently playing in reverse (undo)? */
bool anim_queue_is_reversing(const AnimQueue* aq);

/* Get the Minotaur's movement direction for the current step.
 * Returns direction as -1/0/+1 for col (X) and row (Z).
 * Only meaningful during MINOTAUR_STEP1/STEP2 phases. */
void anim_queue_minotaur_dir(const AnimQueue* aq,
                              int* out_dir_col, int* out_dir_row);

/* Get the raw tween progress (0→1) for the current minotaur step.
 * Returns 0 if not in a minotaur phase. */
float anim_queue_minotaur_progress(const AnimQueue* aq);

/* Is the minotaur currently in a teleport sub-phase? */
bool anim_queue_is_minotaur_teleporting(const AnimQueue* aq);

/* Minotaur teleport effect progress (0→1 for each half).
 * Returns -1 if not in minotaur teleport.
 * out_phase: 0 = beaming out, 1 = beaming in */
float anim_queue_minotaur_teleport_progress(const AnimQueue* aq, int* out_phase);

/*
 * Enable/disable fast-forward mode.
 * When enabled, animation dt is multiplied by g_settings.anim_speed
 * (user-configurable, 1.0–4.0) so remaining animations complete faster
 * when the player has buffered input. Works for both forward and reverse
 * (undo) playback.
 */
void anim_queue_set_fast_forward(AnimQueue* aq, bool fast);

/* ── Win animation queries ──────────────────────── */

/*
 * Get the current win animation sub-phase.
 * Returns ANIM_PHASE_WIN_EXIT, ANIM_PHASE_WIN_GATE, or ANIM_PHASE_IDLE
 * if not in a win animation.
 */
AnimPhase anim_queue_win_phase(const AnimQueue* aq);

/* Get the exit direction for the win animation. */
Direction anim_queue_win_exit_dir(const AnimQueue* aq);

/*
 * Get interpolated Theseus position during win exit hop.
 * Valid during ANIM_PHASE_WIN_EXIT; otherwise returns final position
 * from the record.
 */
void anim_queue_win_exit_pos(const AnimQueue* aq,
                              float* out_col, float* out_row,
                              float* out_hop);

/* Get the win gate lock progress (0→1). Returns -1 if not in gate phase. */
float anim_queue_win_gate_progress(const AnimQueue* aq);

/* Was the win achieved with optimal turn count? */
bool anim_queue_win_is_optimal(const AnimQueue* aq);

/*
 * Get the celebration bounce progress (0→1, loops).
 * Returns -1 if not in celebrate phase.
 */
float anim_queue_win_celebrate_progress(const AnimQueue* aq);

/*
 * Stop the celebration loop (transitions to IDLE).
 * Called by 6.8c when camera transition completes and old scene can be torn down.
 */
void anim_queue_win_stop_celebrate(AnimQueue* aq);

/*
 * Set the optimal flag for the win celebration.
 * Call after anim_queue_start() when the result is TURN_RESULT_WIN.
 * Controls whether the celebrate phase includes a 360° spin.
 */
void anim_queue_win_set_optimal(AnimQueue* aq, bool is_optimal);

#endif /* ENGINE_ANIM_QUEUE_H */
