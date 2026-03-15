#include "engine/anim_queue.h"
#include "data/settings.h"
#include <string.h>

/* ── Helpers: find events by phase ────────────────────── */

/*
 * Find the next event of the given phase starting at start_idx.
 * Returns the event index, or -1 if none found.
 */
static int find_next_event(const TurnRecord* r, AnimEventPhase phase, int start_idx) {
    for (int i = start_idx; i < r->event_count; i++) {
        if (r->events[i].phase == phase) return i;
    }
    return -1;
}

/*
 * Find the first Theseus-phase event that determines the move type.
 */
static const AnimEvent* find_theseus_move_event(const TurnRecord* r) {
    for (int i = 0; i < r->event_count; i++) {
        const AnimEvent* e = &r->events[i];
        if (e->phase != ANIM_EVENT_PHASE_THESEUS) continue;
        switch (e->type) {
        case ANIM_EVT_THESEUS_HOP:
        case ANIM_EVT_THESEUS_ICE_SLIDE:
        case ANIM_EVT_THESEUS_TELEPORT:
        case ANIM_EVT_THESEUS_PUSH_MOVE:
        case ANIM_EVT_BOX_SLIDE:
        case ANIM_EVT_TURNSTILE_ROTATE:
            return e;
        default:
            break;
        }
    }
    return NULL;
}

static const AnimEvent* find_event_of_type(const TurnRecord* r, AnimEventType type) {
    for (int i = 0; i < r->event_count; i++) {
        if (r->events[i].type == type) return &r->events[i];
    }
    return NULL;
}

/* ── Phase starters ───────────────────────────────────── */

static void start_theseus_phase(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;
    aq->phase = ANIM_PHASE_THESEUS;

    /* Determine Theseus move type from events */
    const AnimEvent* move_evt = find_theseus_move_event(r);

    if (!r->theseus_moved && !r->theseus_pushed) {
        /* Wait — no movement, skip through instantly */
        aq->theseus_event_type = ANIM_EVT_NONE;
        tween_init(&aq->move_x, (float)r->theseus_from_col,
                   (float)r->theseus_to_col, 0.001f, ease_linear);
        tween_init(&aq->move_y, (float)r->theseus_from_row,
                   (float)r->theseus_to_row, 0.001f, ease_linear);
        tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
        return;
    }

    if (move_evt && move_evt->type == ANIM_EVT_THESEUS_ICE_SLIDE) {
        /* Ice slide: hop to first ice tile, then slide through waypoints */
        aq->theseus_event_type = ANIM_EVT_THESEUS_ICE_SLIDE;
        aq->theseus_sub = THESEUS_SUB_HOP;

        /* Copy waypoints */
        aq->ice_wp_count = move_evt->ice_slide.waypoint_count;
        memcpy(aq->ice_wp_cols, move_evt->ice_slide.waypoint_cols,
               sizeof(int) * (size_t)aq->ice_wp_count);
        memcpy(aq->ice_wp_rows, move_evt->ice_slide.waypoint_rows,
               sizeof(int) * (size_t)aq->ice_wp_count);
        aq->ice_wp_index = 0;

        /* Phase 1: hop to first ice tile (waypoint 0) */
        tween_init(&aq->move_x, (float)r->theseus_from_col,
                   (float)aq->ice_wp_cols[0],
                   ANIM_THESEUS_DURATION, ease_out_cubic);
        tween_init(&aq->move_y, (float)r->theseus_from_row,
                   (float)aq->ice_wp_rows[0],
                   ANIM_THESEUS_DURATION, ease_out_cubic);
        tween_init(&aq->hop, 0.0f, 1.0f,
                   ANIM_THESEUS_DURATION, ease_parabolic_arc);
        return;
    }

    if (move_evt && move_evt->type == ANIM_EVT_THESEUS_TELEPORT) {
        /* Teleport: fade out then fade in */
        aq->theseus_event_type = ANIM_EVT_THESEUS_TELEPORT;
        aq->theseus_sub = THESEUS_SUB_TELEPORT_OUT;
        tween_init(&aq->effect, 0.0f, 1.0f, ANIM_TELEPORT_HALF, ease_in_quad);
        /* Position stays at source during fade out */
        tween_init(&aq->move_x, (float)move_evt->from_col,
                   (float)move_evt->from_col, ANIM_TELEPORT_HALF, ease_linear);
        tween_init(&aq->move_y, (float)move_evt->from_row,
                   (float)move_evt->from_row, ANIM_TELEPORT_HALF, ease_linear);
        tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
        return;
    }

    if (r->theseus_pushed) {
        /* Push — check what kind */
        const AnimEvent* box_evt = find_event_of_type(r, ANIM_EVT_BOX_SLIDE);
        const AnimEvent* push_evt = find_event_of_type(r, ANIM_EVT_THESEUS_PUSH_MOVE);
        const AnimEvent* ts_evt = find_event_of_type(r, ANIM_EVT_TURNSTILE_ROTATE);

        if (ts_evt) {
            /* Manual turnstile rotation */
            aq->theseus_event_type = ANIM_EVT_TURNSTILE_ROTATE;
            aq->theseus_sub = THESEUS_SUB_PUSH;
            float dur = ANIM_TURNSTILE_DURATION;
            tween_init(&aq->move_x, (float)ts_evt->from_col,
                       (float)ts_evt->to_col, dur, ease_in_out_quad);
            tween_init(&aq->move_y, (float)ts_evt->from_row,
                       (float)ts_evt->to_row, dur, ease_in_out_quad);
            tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
            tween_init(&aq->rotation, 0.0f, 1.0f, dur, ease_in_out_quad);
            return;
        }

        if (box_evt && push_evt) {
            /* Groove box push — visual multi-phase effect is handled
             * in puzzle_scene rendering by remapping tween progress.
             * Here we just set up standard tweens over a longer duration. */
            aq->theseus_event_type = ANIM_EVT_BOX_SLIDE;
            aq->theseus_sub = THESEUS_SUB_PUSH;
            float dur = ANIM_PUSH_DURATION * 2.5f; /* ~0.375s for 3-phase feel */
            tween_init(&aq->move_x, (float)push_evt->from_col,
                       (float)push_evt->to_col, dur, ease_linear);
            tween_init(&aq->move_y, (float)push_evt->from_row,
                       (float)push_evt->to_row, dur, ease_linear);
            tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
            tween_init(&aq->aux_x, (float)box_evt->box.box_from_col,
                       (float)box_evt->box.box_to_col, dur, ease_linear);
            tween_init(&aq->aux_y, (float)box_evt->box.box_from_row,
                       (float)box_evt->box.box_to_row, dur, ease_linear);
            return;
        }

        /* Fallback for unknown push */
        aq->theseus_event_type = ANIM_EVT_NONE;
        tween_init(&aq->move_x, (float)r->theseus_from_col,
                   (float)r->theseus_to_col, 0.001f, ease_linear);
        tween_init(&aq->move_y, (float)r->theseus_from_row,
                   (float)r->theseus_to_row, 0.001f, ease_linear);
        tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
        return;
    }

    /* Normal hop */
    aq->theseus_event_type = ANIM_EVT_THESEUS_HOP;
    aq->theseus_sub = THESEUS_SUB_HOP;
    tween_init(&aq->move_x, (float)r->theseus_from_col,
               (float)r->theseus_to_col,
               ANIM_THESEUS_DURATION, ease_out_cubic);
    tween_init(&aq->move_y, (float)r->theseus_from_row,
               (float)r->theseus_to_row,
               ANIM_THESEUS_DURATION, ease_out_cubic);
    tween_init(&aq->hop, 0.0f, 1.0f,
               ANIM_THESEUS_DURATION, ease_parabolic_arc);
}

static bool start_theseus_effects_phase(AnimQueue* aq) {
    int idx = find_next_event(&aq->record, ANIM_EVENT_PHASE_THESEUS_EFFECT, 0);
    if (idx < 0) return false;

    aq->phase = ANIM_PHASE_THESEUS_EFFECTS;
    aq->effect_event_idx = idx;

    const AnimEvent* e = &aq->record.events[idx];
    float dur = ANIM_CRUMBLE_DURATION;
    switch (e->type) {
    case ANIM_EVT_FLOOR_CRUMBLE: dur = ANIM_CRUMBLE_DURATION; break;
    case ANIM_EVT_GATE_LOCK:     dur = ANIM_GATE_LOCK_DURATION; break;
    case ANIM_EVT_PLATE_TOGGLE:  dur = ANIM_PLATE_DURATION; break;
    default: break;
    }
    tween_init(&aq->phase_tween, 0.0f, 1.0f, dur, ease_linear);
    return true;
}

static float env_event_duration(const AnimEvent* e) {
    switch (e->type) {
    case ANIM_EVT_SPIKE_CHANGE:          return ANIM_SPIKE_DURATION;
    case ANIM_EVT_AUTO_TURNSTILE_ROTATE: return ANIM_AUTO_TURNSTILE_DURATION;
    case ANIM_EVT_PLATFORM_MOVE:         return ANIM_PLATFORM_DURATION;
    case ANIM_EVT_CONVEYOR_PUSH:         return ANIM_CONVEYOR_DURATION;
    default:                             return ANIM_ENVIRONMENT_DURATION;
    }
}

static void setup_env_event_tweens(AnimQueue* aq, const AnimEvent* e, float dur) {
    tween_init(&aq->phase_tween, 0.0f, 1.0f, dur, ease_linear);

    if (e->type == ANIM_EVT_PLATFORM_MOVE) {
        tween_init(&aq->aux_x, (float)e->platform.platform_from_col,
                   (float)e->platform.platform_to_col, dur, ease_in_out_quad);
        tween_init(&aq->aux_y, (float)e->platform.platform_from_row,
                   (float)e->platform.platform_to_row, dur, ease_in_out_quad);
    } else if (e->type == ANIM_EVT_CONVEYOR_PUSH) {
        tween_init(&aq->aux_x, (float)e->from_col, (float)e->to_col,
                   dur, ease_linear);
        tween_init(&aq->aux_y, (float)e->from_row, (float)e->to_row,
                   dur, ease_linear);
    } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
        tween_init(&aq->rotation, 0.0f, 1.0f, dur, ease_in_out_quad);
    }
}

static bool start_environment_phase(AnimQueue* aq) {
    int idx = find_next_event(&aq->record, ANIM_EVENT_PHASE_ENVIRONMENT, 0);

    aq->phase = ANIM_PHASE_ENVIRONMENT;

    /* Initialize actor tracking positions for the environment phase.
     * These start at pre-environment positions and get updated as
     * env events move actors, preventing snap-back between events. */
    aq->env_theseus_col  = (float)aq->record.theseus_to_col;
    aq->env_theseus_row  = (float)aq->record.theseus_to_row;
    aq->env_minotaur_col = (float)aq->record.minotaur_start_col;
    aq->env_minotaur_row = (float)aq->record.minotaur_start_row;

    if (idx < 0) {
        /* No environment events — minimum pause */
        aq->effect_event_idx = -1;
        tween_init(&aq->phase_tween, 0.0f, 1.0f,
                   ANIM_ENV_MIN_PAUSE, ease_linear);
        return true;
    }

    aq->effect_event_idx = idx;
    const AnimEvent* e = &aq->record.events[idx];
    float dur = env_event_duration(e);
    setup_env_event_tweens(aq, e, dur);
    return true;
}

static void start_minotaur_step1(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;
    aq->phase = ANIM_PHASE_MINOTAUR_STEP1;

    /* Use env_minotaur tracking position as the start, since env events
     * may have moved the minotaur (conveyor, platform, auto-turnstile)
     * after minotaur_start_col/row was recorded. */
    float start_col = aq->env_minotaur_col;
    float start_row = aq->env_minotaur_row;

    if (r->minotaur_steps >= 1) {
        tween_init(&aq->mino_x, start_col,
                   (float)r->minotaur_after1_col,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
        tween_init(&aq->mino_y, start_row,
                   (float)r->minotaur_after1_row,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
        /* Track movement direction for roll animation */
        aq->mino_dir_col = r->minotaur_after1_col - (int)(start_col + 0.5f);
        aq->mino_dir_row = r->minotaur_after1_row - (int)(start_row + 0.5f);
    } else {
        tween_init(&aq->mino_x, start_col,
                   start_col, 0.001f, ease_linear);
        tween_init(&aq->mino_y, start_row,
                   start_row, 0.001f, ease_linear);
        aq->mino_dir_col = 0;
        aq->mino_dir_row = 0;
    }
}

static void start_minotaur_step2(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;
    aq->phase = ANIM_PHASE_MINOTAUR_STEP2;

    if (r->minotaur_steps >= 2) {
        tween_init(&aq->mino_x, (float)r->minotaur_after1_col,
                   (float)r->minotaur_after2_col,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
        tween_init(&aq->mino_y, (float)r->minotaur_after1_row,
                   (float)r->minotaur_after2_row,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
        /* Track movement direction for roll animation */
        aq->mino_dir_col = r->minotaur_after2_col - r->minotaur_after1_col;
        aq->mino_dir_row = r->minotaur_after2_row - r->minotaur_after1_row;
    } else {
        tween_init(&aq->mino_x, (float)r->minotaur_after1_col,
                   (float)r->minotaur_after1_col, 0.001f, ease_linear);
        tween_init(&aq->mino_y, (float)r->minotaur_after1_row,
                   (float)r->minotaur_after1_row, 0.001f, ease_linear);
        aq->mino_dir_col = 0;
        aq->mino_dir_row = 0;
    }
}

/*
 * Update env_theseus/minotaur tracking positions after the current
 * env event finishes animating.  This prevents actors from snapping
 * back to their pre-environment positions between events.
 */
static void update_env_tracking(AnimQueue* aq) {
    const AnimEvent* e = anim_queue_current_event(aq);
    if (!e) return;

    if (e->type == ANIM_EVT_CONVEYOR_PUSH) {
        if (e->entity == ENTITY_THESEUS) {
            aq->env_theseus_col = (float)e->to_col;
            aq->env_theseus_row = (float)e->to_row;
        }
        if (e->entity == ENTITY_MINOTAUR) {
            aq->env_minotaur_col = (float)e->to_col;
            aq->env_minotaur_row = (float)e->to_row;
        }
    } else if (e->type == ANIM_EVT_PLATFORM_MOVE) {
        if (e->platform.theseus_riding) {
            aq->env_theseus_col = (float)e->platform.platform_to_col;
            aq->env_theseus_row = (float)e->platform.platform_to_row;
        }
        if (e->platform.minotaur_riding) {
            aq->env_minotaur_col = (float)e->platform.platform_to_col;
            aq->env_minotaur_row = (float)e->platform.platform_to_row;
        }
    } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
        if (e->turnstile.actor_moved[0]) {
            aq->env_theseus_col = (float)e->turnstile.actor_to_col[0];
            aq->env_theseus_row = (float)e->turnstile.actor_to_row[0];
        }
        if (e->turnstile.actor_moved[1]) {
            aq->env_minotaur_col = (float)e->turnstile.actor_to_col[1];
            aq->env_minotaur_row = (float)e->turnstile.actor_to_row[1];
        }
    }
}

/* Advance to next environment event, or return false if done */
static bool advance_env_event(AnimQueue* aq) {
    int next = find_next_event(&aq->record, ANIM_EVENT_PHASE_ENVIRONMENT,
                               aq->effect_event_idx + 1);
    if (next < 0) return false;

    aq->effect_event_idx = next;
    const AnimEvent* e = &aq->record.events[next];
    float dur = env_event_duration(e);
    setup_env_event_tweens(aq, e, dur);
    return true;
}

/* Advance to next theseus-effect event, or return false if done */
static bool advance_effect_event(AnimQueue* aq) {
    int next = find_next_event(&aq->record, ANIM_EVENT_PHASE_THESEUS_EFFECT,
                               aq->effect_event_idx + 1);
    if (next < 0) return false;

    aq->effect_event_idx = next;
    const AnimEvent* e = &aq->record.events[next];
    float dur = ANIM_CRUMBLE_DURATION;
    switch (e->type) {
    case ANIM_EVT_FLOOR_CRUMBLE: dur = ANIM_CRUMBLE_DURATION; break;
    case ANIM_EVT_GATE_LOCK:     dur = ANIM_GATE_LOCK_DURATION; break;
    case ANIM_EVT_PLATE_TOGGLE:  dur = ANIM_PLATE_DURATION; break;
    default: break;
    }
    tween_init(&aq->phase_tween, 0.0f, 1.0f, dur, ease_linear);
    return true;
}

/* ── Ice slide sub-phase transition ──────────────────── */

static void start_ice_slide_sub(AnimQueue* aq) {
    aq->theseus_sub = THESEUS_SUB_ICE_SLIDE;
    aq->ice_wp_index = 1;

    if (aq->ice_wp_count <= 1) return;

    tween_init(&aq->move_x, (float)aq->ice_wp_cols[0],
               (float)aq->ice_wp_cols[1],
               ANIM_ICE_SLIDE_PER_TILE, ease_linear);
    tween_init(&aq->move_y, (float)aq->ice_wp_rows[0],
               (float)aq->ice_wp_rows[1],
               ANIM_ICE_SLIDE_PER_TILE, ease_linear);
    tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
}

/* ── Reverse playback helpers ─────────────────────────── */

/*
 * Find the LAST event of a given phase.
 * Returns the event index, or -1 if none found.
 */
static int find_last_event(const TurnRecord* r, AnimEventPhase phase) {
    for (int i = r->event_count - 1; i >= 0; i--) {
        if (r->events[i].phase == phase) return i;
    }
    return -1;
}

/*
 * Find the previous event of the given phase before start_idx.
 * Returns the event index, or -1 if none found.
 */
static int find_prev_event(const TurnRecord* r, AnimEventPhase phase, int start_idx) {
    for (int i = start_idx - 1; i >= 0; i--) {
        if (r->events[i].phase == phase) return i;
    }
    return -1;
}

/*
 * Apply speed multiplier to a duration for reverse playback.
 */
static float rev_dur(float dur) {
    return dur / ANIM_REVERSE_SPEED;
}

/* Start reverse minotaur step 2 (first phase of reverse playback) */
static void start_reverse_minotaur_step2(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;
    aq->phase = ANIM_PHASE_MINOTAUR_STEP2;

    if (r->minotaur_steps >= 2) {
        /* Reverse: after2 → after1 */
        float dur = rev_dur(ANIM_MINOTAUR_DURATION);
        tween_init(&aq->mino_x, (float)r->minotaur_after2_col,
                   (float)r->minotaur_after1_col, dur, ease_in_out_quad);
        tween_init(&aq->mino_y, (float)r->minotaur_after2_row,
                   (float)r->minotaur_after1_row, dur, ease_in_out_quad);
        aq->mino_dir_col = r->minotaur_after1_col - r->minotaur_after2_col;
        aq->mino_dir_row = r->minotaur_after1_row - r->minotaur_after2_row;
    } else {
        tween_init(&aq->mino_x, (float)r->minotaur_after2_col,
                   (float)r->minotaur_after2_col, 0.001f, ease_linear);
        tween_init(&aq->mino_y, (float)r->minotaur_after2_row,
                   (float)r->minotaur_after2_row, 0.001f, ease_linear);
        aq->mino_dir_col = 0;
        aq->mino_dir_row = 0;
    }
}

/* Start reverse minotaur step 1: after1 → env_minotaur (pre-mino position) */
static void start_reverse_minotaur_step1(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;
    aq->phase = ANIM_PHASE_MINOTAUR_STEP1;

    if (r->minotaur_steps >= 1) {
        float dur = rev_dur(ANIM_MINOTAUR_DURATION);
        tween_init(&aq->mino_x, (float)r->minotaur_after1_col,
                   aq->env_minotaur_col, dur, ease_in_out_quad);
        tween_init(&aq->mino_y, (float)r->minotaur_after1_row,
                   aq->env_minotaur_row, dur, ease_in_out_quad);
        aq->mino_dir_col = (int)(aq->env_minotaur_col + 0.5f) - r->minotaur_after1_col;
        aq->mino_dir_row = (int)(aq->env_minotaur_row + 0.5f) - r->minotaur_after1_row;
    } else {
        tween_init(&aq->mino_x, (float)r->minotaur_after1_col,
                   (float)r->minotaur_after1_col, 0.001f, ease_linear);
        tween_init(&aq->mino_y, (float)r->minotaur_after1_row,
                   (float)r->minotaur_after1_row, 0.001f, ease_linear);
        aq->mino_dir_col = 0;
        aq->mino_dir_row = 0;
    }
}

/* Setup reverse env event tweens (swap from/to) */
static void setup_reverse_env_event_tweens(AnimQueue* aq, const AnimEvent* e, float dur) {
    tween_init(&aq->phase_tween, 0.0f, 1.0f, dur, ease_linear);

    if (e->type == ANIM_EVT_PLATFORM_MOVE) {
        tween_init(&aq->aux_x, (float)e->platform.platform_to_col,
                   (float)e->platform.platform_from_col, dur, ease_in_out_quad);
        tween_init(&aq->aux_y, (float)e->platform.platform_to_row,
                   (float)e->platform.platform_from_row, dur, ease_in_out_quad);
    } else if (e->type == ANIM_EVT_CONVEYOR_PUSH) {
        tween_init(&aq->aux_x, (float)e->to_col, (float)e->from_col,
                   dur, ease_linear);
        tween_init(&aq->aux_y, (float)e->to_row, (float)e->from_row,
                   dur, ease_linear);
    } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
        tween_init(&aq->rotation, 1.0f, 0.0f, dur, ease_in_out_quad);
    }
}

/* Start reverse environment phase: iterate env events in reverse order */
static bool start_reverse_environment_phase(AnimQueue* aq) {
    int idx = find_last_event(&aq->record, ANIM_EVENT_PHASE_ENVIRONMENT);

    aq->phase = ANIM_PHASE_ENVIRONMENT;

    /* Initialize env tracking at POST-environment positions (end state).
     * During reverse playback, we start from the final env positions and
     * track backward. */
    aq->env_theseus_col  = (float)aq->record.theseus_to_col;
    aq->env_theseus_row  = (float)aq->record.theseus_to_row;
    aq->env_minotaur_col = (float)aq->record.minotaur_start_col;
    aq->env_minotaur_row = (float)aq->record.minotaur_start_row;

    /* Walk forward through all env events to find final env positions */
    for (int i = 0; i < aq->record.event_count; i++) {
        const AnimEvent* e = &aq->record.events[i];
        if (e->phase != ANIM_EVENT_PHASE_ENVIRONMENT) continue;
        if (e->type == ANIM_EVT_CONVEYOR_PUSH) {
            if (e->entity == ENTITY_THESEUS) {
                aq->env_theseus_col = (float)e->to_col;
                aq->env_theseus_row = (float)e->to_row;
            }
            if (e->entity == ENTITY_MINOTAUR) {
                aq->env_minotaur_col = (float)e->to_col;
                aq->env_minotaur_row = (float)e->to_row;
            }
        } else if (e->type == ANIM_EVT_PLATFORM_MOVE) {
            if (e->platform.theseus_riding) {
                aq->env_theseus_col = (float)e->platform.platform_to_col;
                aq->env_theseus_row = (float)e->platform.platform_to_row;
            }
            if (e->platform.minotaur_riding) {
                aq->env_minotaur_col = (float)e->platform.platform_to_col;
                aq->env_minotaur_row = (float)e->platform.platform_to_row;
            }
        } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
            if (e->turnstile.actor_moved[0]) {
                aq->env_theseus_col = (float)e->turnstile.actor_to_col[0];
                aq->env_theseus_row = (float)e->turnstile.actor_to_row[0];
            }
            if (e->turnstile.actor_moved[1]) {
                aq->env_minotaur_col = (float)e->turnstile.actor_to_col[1];
                aq->env_minotaur_row = (float)e->turnstile.actor_to_row[1];
            }
        }
    }

    if (idx < 0) {
        /* No environment events — minimum pause */
        aq->effect_event_idx = -1;
        tween_init(&aq->phase_tween, 0.0f, 1.0f,
                   rev_dur(ANIM_ENV_MIN_PAUSE), ease_linear);
        return true;
    }

    aq->effect_event_idx = idx;
    const AnimEvent* e = &aq->record.events[idx];
    float dur = rev_dur(env_event_duration(e));
    setup_reverse_env_event_tweens(aq, e, dur);
    return true;
}

/* Update reverse env tracking BEFORE advancing to previous event.
 * Undoes the effect of the current event on actor positions. */
static void update_reverse_env_tracking(AnimQueue* aq) {
    const AnimEvent* e = anim_queue_current_event(aq);
    if (!e) return;

    /* Reverse: set tracking back to pre-event positions (from instead of to) */
    if (e->type == ANIM_EVT_CONVEYOR_PUSH) {
        if (e->entity == ENTITY_THESEUS) {
            aq->env_theseus_col = (float)e->from_col;
            aq->env_theseus_row = (float)e->from_row;
        }
        if (e->entity == ENTITY_MINOTAUR) {
            aq->env_minotaur_col = (float)e->from_col;
            aq->env_minotaur_row = (float)e->from_row;
        }
    } else if (e->type == ANIM_EVT_PLATFORM_MOVE) {
        if (e->platform.theseus_riding) {
            aq->env_theseus_col = (float)e->platform.platform_from_col;
            aq->env_theseus_row = (float)e->platform.platform_from_row;
        }
        if (e->platform.minotaur_riding) {
            aq->env_minotaur_col = (float)e->platform.platform_from_col;
            aq->env_minotaur_row = (float)e->platform.platform_from_row;
        }
    } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
        if (e->turnstile.actor_moved[0]) {
            aq->env_theseus_col = (float)e->turnstile.actor_from_col[0];
            aq->env_theseus_row = (float)e->turnstile.actor_from_row[0];
        }
        if (e->turnstile.actor_moved[1]) {
            aq->env_minotaur_col = (float)e->turnstile.actor_from_col[1];
            aq->env_minotaur_row = (float)e->turnstile.actor_from_row[1];
        }
    }
}

/* Advance to PREVIOUS env event (reverse iteration) */
static bool advance_reverse_env_event(AnimQueue* aq) {
    int prev = find_prev_event(&aq->record, ANIM_EVENT_PHASE_ENVIRONMENT,
                                aq->effect_event_idx);
    if (prev < 0) return false;

    aq->effect_event_idx = prev;
    const AnimEvent* e = &aq->record.events[prev];
    float dur = rev_dur(env_event_duration(e));
    setup_reverse_env_event_tweens(aq, e, dur);
    return true;
}

/* Start reverse theseus effects phase: iterate in reverse */
static bool start_reverse_theseus_effects_phase(AnimQueue* aq) {
    int idx = find_last_event(&aq->record, ANIM_EVENT_PHASE_THESEUS_EFFECT);
    if (idx < 0) return false;

    aq->phase = ANIM_PHASE_THESEUS_EFFECTS;
    aq->effect_event_idx = idx;

    const AnimEvent* e = &aq->record.events[idx];
    float dur = ANIM_CRUMBLE_DURATION;
    switch (e->type) {
    case ANIM_EVT_FLOOR_CRUMBLE: dur = ANIM_CRUMBLE_DURATION; break;
    case ANIM_EVT_GATE_LOCK:     dur = ANIM_GATE_LOCK_DURATION; break;
    case ANIM_EVT_PLATE_TOGGLE:  dur = ANIM_PLATE_DURATION; break;
    default: break;
    }
    tween_init(&aq->phase_tween, 1.0f, 0.0f, rev_dur(dur), ease_linear);
    return true;
}

/* Advance to previous theseus-effect event */
static bool advance_reverse_effect_event(AnimQueue* aq) {
    int prev = find_prev_event(&aq->record, ANIM_EVENT_PHASE_THESEUS_EFFECT,
                                aq->effect_event_idx);
    if (prev < 0) return false;

    aq->effect_event_idx = prev;
    const AnimEvent* e = &aq->record.events[prev];
    float dur = ANIM_CRUMBLE_DURATION;
    switch (e->type) {
    case ANIM_EVT_FLOOR_CRUMBLE: dur = ANIM_CRUMBLE_DURATION; break;
    case ANIM_EVT_GATE_LOCK:     dur = ANIM_GATE_LOCK_DURATION; break;
    case ANIM_EVT_PLATE_TOGGLE:  dur = ANIM_PLATE_DURATION; break;
    default: break;
    }
    tween_init(&aq->phase_tween, 1.0f, 0.0f, rev_dur(dur), ease_linear);
    return true;
}

/* Start reverse Theseus phase: swap from/to */
static void start_reverse_theseus_phase(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;
    aq->phase = ANIM_PHASE_THESEUS;

    const AnimEvent* move_evt = find_theseus_move_event(r);

    if (!r->theseus_moved && !r->theseus_pushed) {
        aq->theseus_event_type = ANIM_EVT_NONE;
        tween_init(&aq->move_x, (float)r->theseus_to_col,
                   (float)r->theseus_from_col, 0.001f, ease_linear);
        tween_init(&aq->move_y, (float)r->theseus_to_row,
                   (float)r->theseus_from_row, 0.001f, ease_linear);
        tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
        return;
    }

    if (move_evt && move_evt->type == ANIM_EVT_THESEUS_ICE_SLIDE) {
        /* Reverse ice slide: slide back from final waypoint to first, then hop back */
        aq->theseus_event_type = ANIM_EVT_THESEUS_ICE_SLIDE;

        /* Copy waypoints in reverse */
        int wpc = move_evt->ice_slide.waypoint_count;
        aq->ice_wp_count = wpc;
        for (int i = 0; i < wpc; i++) {
            aq->ice_wp_cols[i] = move_evt->ice_slide.waypoint_cols[wpc - 1 - i];
            aq->ice_wp_rows[i] = move_evt->ice_slide.waypoint_rows[wpc - 1 - i];
        }
        aq->ice_wp_index = 0;

        if (wpc <= 1) {
            /* Only one waypoint — just hop back */
            aq->theseus_sub = THESEUS_SUB_HOP;
            float dur = rev_dur(ANIM_THESEUS_DURATION);
            tween_init(&aq->move_x, (float)r->theseus_to_col,
                       (float)r->theseus_from_col, dur, ease_out_cubic);
            tween_init(&aq->move_y, (float)r->theseus_to_row,
                       (float)r->theseus_from_row, dur, ease_out_cubic);
            tween_init(&aq->hop, 0.0f, 1.0f, dur, ease_parabolic_arc);
        } else {
            /* Start sliding backwards (no hop during slide phase) */
            aq->theseus_sub = THESEUS_SUB_ICE_SLIDE;
            aq->ice_wp_index = 1;
            float dur = rev_dur(ANIM_ICE_SLIDE_PER_TILE);
            tween_init(&aq->move_x, (float)aq->ice_wp_cols[0],
                       (float)aq->ice_wp_cols[1], dur, ease_linear);
            tween_init(&aq->move_y, (float)aq->ice_wp_rows[0],
                       (float)aq->ice_wp_rows[1], dur, ease_linear);
            tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
        }
        return;
    }

    if (move_evt && move_evt->type == ANIM_EVT_THESEUS_TELEPORT) {
        /* Reverse teleport: fade out at destination, fade in at source */
        aq->theseus_event_type = ANIM_EVT_THESEUS_TELEPORT;
        aq->theseus_sub = THESEUS_SUB_TELEPORT_OUT;
        float dur = rev_dur(ANIM_TELEPORT_HALF);
        tween_init(&aq->effect, 0.0f, 1.0f, dur, ease_in_quad);
        tween_init(&aq->move_x, (float)move_evt->to_col,
                   (float)move_evt->to_col, dur, ease_linear);
        tween_init(&aq->move_y, (float)move_evt->to_row,
                   (float)move_evt->to_row, dur, ease_linear);
        tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
        return;
    }

    if (r->theseus_pushed) {
        const AnimEvent* box_evt = find_event_of_type(r, ANIM_EVT_BOX_SLIDE);
        const AnimEvent* push_evt = find_event_of_type(r, ANIM_EVT_THESEUS_PUSH_MOVE);
        const AnimEvent* ts_evt = find_event_of_type(r, ANIM_EVT_TURNSTILE_ROTATE);

        if (ts_evt) {
            aq->theseus_event_type = ANIM_EVT_TURNSTILE_ROTATE;
            aq->theseus_sub = THESEUS_SUB_PUSH;
            float dur = rev_dur(ANIM_TURNSTILE_DURATION);
            tween_init(&aq->move_x, (float)ts_evt->to_col,
                       (float)ts_evt->from_col, dur, ease_in_out_quad);
            tween_init(&aq->move_y, (float)ts_evt->to_row,
                       (float)ts_evt->from_row, dur, ease_in_out_quad);
            tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
            tween_init(&aq->rotation, 1.0f, 0.0f, dur, ease_in_out_quad);
            return;
        }

        if (box_evt && push_evt) {
            aq->theseus_event_type = ANIM_EVT_BOX_SLIDE;
            aq->theseus_sub = THESEUS_SUB_PUSH;
            float dur = rev_dur(ANIM_PUSH_DURATION * 2.5f);
            tween_init(&aq->move_x, (float)push_evt->to_col,
                       (float)push_evt->from_col, dur, ease_linear);
            tween_init(&aq->move_y, (float)push_evt->to_row,
                       (float)push_evt->from_row, dur, ease_linear);
            tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
            tween_init(&aq->aux_x, (float)box_evt->box.box_to_col,
                       (float)box_evt->box.box_from_col, dur, ease_linear);
            tween_init(&aq->aux_y, (float)box_evt->box.box_to_row,
                       (float)box_evt->box.box_from_row, dur, ease_linear);
            return;
        }

        aq->theseus_event_type = ANIM_EVT_NONE;
        tween_init(&aq->move_x, (float)r->theseus_to_col,
                   (float)r->theseus_from_col, 0.001f, ease_linear);
        tween_init(&aq->move_y, (float)r->theseus_to_row,
                   (float)r->theseus_from_row, 0.001f, ease_linear);
        tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
        return;
    }

    /* Normal hop reversed: to → from */
    aq->theseus_event_type = ANIM_EVT_THESEUS_HOP;
    aq->theseus_sub = THESEUS_SUB_HOP;
    float dur = rev_dur(ANIM_THESEUS_DURATION);
    tween_init(&aq->move_x, (float)r->theseus_to_col,
               (float)r->theseus_from_col, dur, ease_out_cubic);
    tween_init(&aq->move_y, (float)r->theseus_to_row,
               (float)r->theseus_from_row, dur, ease_out_cubic);
    tween_init(&aq->hop, 0.0f, 1.0f, dur, ease_parabolic_arc);
}

/* ── Public API ────────────────────────────────────────── */

void anim_queue_init(AnimQueue* aq) {
    memset(aq, 0, sizeof(*aq));
    aq->phase   = ANIM_PHASE_IDLE;
    aq->playing = false;
}

void anim_queue_start(AnimQueue* aq, const TurnRecord* record) {
    aq->record    = *record;
    aq->playing   = true;
    aq->reversing = false;
    aq->effect_event_idx = -1;

    start_theseus_phase(aq);
}

void anim_queue_start_reverse(AnimQueue* aq, const TurnRecord* record) {
    aq->record    = *record;
    aq->playing   = true;
    aq->reversing = true;
    aq->effect_event_idx = -1;

    /* Compute final env positions so minotaur step reversal starts correctly */
    aq->env_theseus_col  = (float)record->theseus_to_col;
    aq->env_theseus_row  = (float)record->theseus_to_row;
    aq->env_minotaur_col = (float)record->minotaur_start_col;
    aq->env_minotaur_row = (float)record->minotaur_start_row;

    /* Walk env events forward to find final env positions */
    for (int i = 0; i < record->event_count; i++) {
        const AnimEvent* e = &record->events[i];
        if (e->phase != ANIM_EVENT_PHASE_ENVIRONMENT) continue;
        if (e->type == ANIM_EVT_CONVEYOR_PUSH) {
            if (e->entity == ENTITY_THESEUS) {
                aq->env_theseus_col = (float)e->to_col;
                aq->env_theseus_row = (float)e->to_row;
            }
            if (e->entity == ENTITY_MINOTAUR) {
                aq->env_minotaur_col = (float)e->to_col;
                aq->env_minotaur_row = (float)e->to_row;
            }
        } else if (e->type == ANIM_EVT_PLATFORM_MOVE) {
            if (e->platform.theseus_riding) {
                aq->env_theseus_col = (float)e->platform.platform_to_col;
                aq->env_theseus_row = (float)e->platform.platform_to_row;
            }
            if (e->platform.minotaur_riding) {
                aq->env_minotaur_col = (float)e->platform.platform_to_col;
                aq->env_minotaur_row = (float)e->platform.platform_to_row;
            }
        } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
            if (e->turnstile.actor_moved[0]) {
                aq->env_theseus_col = (float)e->turnstile.actor_to_col[0];
                aq->env_theseus_row = (float)e->turnstile.actor_to_row[0];
            }
            if (e->turnstile.actor_moved[1]) {
                aq->env_minotaur_col = (float)e->turnstile.actor_to_col[1];
                aq->env_minotaur_row = (float)e->turnstile.actor_to_row[1];
            }
        }
    }

    /* Start with minotaur step 2 (reverse order) */
    if (record->minotaur_steps >= 2) {
        start_reverse_minotaur_step2(aq);
    } else if (record->minotaur_steps >= 1) {
        start_reverse_minotaur_step1(aq);
    } else {
        /* No minotaur movement — go straight to env phase */
        start_reverse_environment_phase(aq);
    }
}

/* ── Reverse update ────────────────────────────────────── */

static void anim_queue_update_reverse(AnimQueue* aq, float dt) {
    switch (aq->phase) {
    case ANIM_PHASE_MINOTAUR_STEP2:
        tween_update(&aq->mino_x, dt);
        tween_update(&aq->mino_y, dt);
        if (aq->mino_x.finished && aq->mino_y.finished) {
            if (aq->record.minotaur_steps >= 1) {
                start_reverse_minotaur_step1(aq);
            } else {
                start_reverse_environment_phase(aq);
            }
        }
        break;

    case ANIM_PHASE_MINOTAUR_STEP1:
        tween_update(&aq->mino_x, dt);
        tween_update(&aq->mino_y, dt);
        if (aq->mino_x.finished && aq->mino_y.finished) {
            start_reverse_environment_phase(aq);
        }
        break;

    case ANIM_PHASE_ENVIRONMENT:
        tween_update(&aq->phase_tween, dt);
        if (aq->effect_event_idx >= 0 &&
            aq->effect_event_idx < aq->record.event_count) {
            const AnimEvent* e = &aq->record.events[aq->effect_event_idx];
            if (e->type == ANIM_EVT_PLATFORM_MOVE ||
                e->type == ANIM_EVT_CONVEYOR_PUSH) {
                tween_update(&aq->aux_x, dt);
                tween_update(&aq->aux_y, dt);
            } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
                tween_update(&aq->rotation, dt);
            }
        }
        if (aq->phase_tween.finished) {
            update_reverse_env_tracking(aq);
            bool has_more = (aq->effect_event_idx >= 0) &&
                            advance_reverse_env_event(aq);
            if (!has_more) {
                if (!start_reverse_theseus_effects_phase(aq)) {
                    start_reverse_theseus_phase(aq);
                }
            }
        }
        break;

    case ANIM_PHASE_THESEUS_EFFECTS:
        tween_update(&aq->phase_tween, dt);
        if (aq->phase_tween.finished) {
            if (!advance_reverse_effect_event(aq)) {
                start_reverse_theseus_phase(aq);
            }
        }
        break;

    case ANIM_PHASE_THESEUS: {
        /* Handle reverse Theseus sub-phases */
        if (aq->theseus_event_type == ANIM_EVT_THESEUS_ICE_SLIDE) {
            if (aq->theseus_sub == THESEUS_SUB_ICE_SLIDE) {
                tween_update(&aq->move_x, dt);
                tween_update(&aq->move_y, dt);
                if (aq->move_x.finished && aq->move_y.finished) {
                    aq->ice_wp_index++;
                    if (aq->ice_wp_index >= aq->ice_wp_count) {
                        /* Done sliding, now hop from first ice tile back to from */
                        aq->theseus_sub = THESEUS_SUB_HOP;
                        float dur = rev_dur(ANIM_THESEUS_DURATION);
                        tween_init(&aq->move_x,
                                   (float)aq->ice_wp_cols[aq->ice_wp_count - 1],
                                   (float)aq->record.theseus_from_col,
                                   dur, ease_out_cubic);
                        tween_init(&aq->move_y,
                                   (float)aq->ice_wp_rows[aq->ice_wp_count - 1],
                                   (float)aq->record.theseus_from_row,
                                   dur, ease_out_cubic);
                        tween_init(&aq->hop, 0.0f, 1.0f, dur, ease_parabolic_arc);
                    } else {
                        int prev = aq->ice_wp_index - 1;
                        int curr = aq->ice_wp_index;
                        float dur = rev_dur(ANIM_ICE_SLIDE_PER_TILE);
                        tween_init(&aq->move_x,
                                   (float)aq->ice_wp_cols[prev],
                                   (float)aq->ice_wp_cols[curr],
                                   dur, ease_linear);
                        tween_init(&aq->move_y,
                                   (float)aq->ice_wp_rows[prev],
                                   (float)aq->ice_wp_rows[curr],
                                   dur, ease_linear);
                    }
                }
            } else {
                /* HOP sub-phase (reverse: hop from first ice tile back to from) */
                tween_update(&aq->move_x, dt);
                tween_update(&aq->move_y, dt);
                tween_update(&aq->hop, dt);
                if (aq->move_x.finished && aq->move_y.finished) {
                    goto reverse_theseus_done;
                }
            }
            break;
        }

        if (aq->theseus_event_type == ANIM_EVT_THESEUS_TELEPORT) {
            tween_update(&aq->effect, dt);
            tween_update(&aq->move_x, dt);
            tween_update(&aq->move_y, dt);
            if (aq->effect.finished) {
                if (aq->theseus_sub == THESEUS_SUB_TELEPORT_OUT) {
                    const AnimEvent* tp = find_event_of_type(&aq->record,
                                                             ANIM_EVT_THESEUS_TELEPORT);
                    aq->theseus_sub = THESEUS_SUB_TELEPORT_IN;
                    float dur = rev_dur(ANIM_TELEPORT_HALF);
                    tween_init(&aq->effect, 0.0f, 1.0f, dur, ease_out_quad);
                    if (tp) {
                        /* Reverse: fade in at source */
                        tween_init(&aq->move_x, (float)tp->from_col,
                                   (float)tp->from_col, dur, ease_linear);
                        tween_init(&aq->move_y, (float)tp->from_row,
                                   (float)tp->from_row, dur, ease_linear);
                    }
                } else {
                    goto reverse_theseus_done;
                }
            }
            break;
        }

        if (aq->theseus_event_type == ANIM_EVT_BOX_SLIDE ||
            aq->theseus_event_type == ANIM_EVT_TURNSTILE_ROTATE) {
            tween_update(&aq->move_x, dt);
            tween_update(&aq->move_y, dt);
            if (aq->theseus_event_type == ANIM_EVT_BOX_SLIDE) {
                tween_update(&aq->aux_x, dt);
                tween_update(&aq->aux_y, dt);
            }
            if (aq->theseus_event_type == ANIM_EVT_TURNSTILE_ROTATE) {
                tween_update(&aq->rotation, dt);
            }
            if (aq->move_x.finished && aq->move_y.finished) {
                goto reverse_theseus_done;
            }
            break;
        }

        /* Normal hop or NONE */
        tween_update(&aq->move_x, dt);
        tween_update(&aq->move_y, dt);
        tween_update(&aq->hop, dt);
        if (aq->move_x.finished && aq->move_y.finished) {
            goto reverse_theseus_done;
        }
        break;

    reverse_theseus_done:
        aq->playing = false;
        aq->phase = ANIM_PHASE_IDLE;
        break;
    }

    case ANIM_PHASE_IDLE:
        break;
    }
}

void anim_queue_update(AnimQueue* aq, float dt) {
    if (!aq->playing) return;

    /* Apply fast-forward speed multiplier when input is buffered.
     * Uses g_settings.anim_speed (user-configurable, 1.0–4.0). */
    if (aq->fast_forward) {
        dt *= g_settings.anim_speed;
    }

    if (aq->reversing) {
        anim_queue_update_reverse(aq, dt);
        return;
    }

    switch (aq->phase) {
    case ANIM_PHASE_THESEUS: {
        /* Handle sub-phases for complex moves */
        if (aq->theseus_event_type == ANIM_EVT_THESEUS_ICE_SLIDE) {
            if (aq->theseus_sub == THESEUS_SUB_HOP) {
                tween_update(&aq->move_x, dt);
                tween_update(&aq->move_y, dt);
                tween_update(&aq->hop, dt);
                if (aq->move_x.finished && aq->move_y.finished) {
                    start_ice_slide_sub(aq);
                    if (aq->ice_wp_count <= 1) goto theseus_done;
                }
            } else {
                tween_update(&aq->move_x, dt);
                tween_update(&aq->move_y, dt);
                if (aq->move_x.finished && aq->move_y.finished) {
                    aq->ice_wp_index++;
                    if (aq->ice_wp_index >= aq->ice_wp_count) {
                        goto theseus_done;
                    }
                    int prev = aq->ice_wp_index - 1;
                    int curr = aq->ice_wp_index;
                    tween_init(&aq->move_x,
                               (float)aq->ice_wp_cols[prev],
                               (float)aq->ice_wp_cols[curr],
                               ANIM_ICE_SLIDE_PER_TILE, ease_linear);
                    tween_init(&aq->move_y,
                               (float)aq->ice_wp_rows[prev],
                               (float)aq->ice_wp_rows[curr],
                               ANIM_ICE_SLIDE_PER_TILE, ease_linear);
                }
            }
            break;
        }

        if (aq->theseus_event_type == ANIM_EVT_THESEUS_TELEPORT) {
            tween_update(&aq->effect, dt);
            tween_update(&aq->move_x, dt);
            tween_update(&aq->move_y, dt);
            if (aq->effect.finished) {
                if (aq->theseus_sub == THESEUS_SUB_TELEPORT_OUT) {
                    const AnimEvent* tp = find_event_of_type(&aq->record,
                                                             ANIM_EVT_THESEUS_TELEPORT);
                    aq->theseus_sub = THESEUS_SUB_TELEPORT_IN;
                    tween_init(&aq->effect, 0.0f, 1.0f,
                               ANIM_TELEPORT_HALF, ease_out_quad);
                    if (tp) {
                        tween_init(&aq->move_x, (float)tp->to_col,
                                   (float)tp->to_col, ANIM_TELEPORT_HALF, ease_linear);
                        tween_init(&aq->move_y, (float)tp->to_row,
                                   (float)tp->to_row, ANIM_TELEPORT_HALF, ease_linear);
                    }
                } else {
                    goto theseus_done;
                }
            }
            break;
        }

        if (aq->theseus_event_type == ANIM_EVT_BOX_SLIDE ||
            aq->theseus_event_type == ANIM_EVT_TURNSTILE_ROTATE) {
            tween_update(&aq->move_x, dt);
            tween_update(&aq->move_y, dt);
            if (aq->theseus_event_type == ANIM_EVT_BOX_SLIDE) {
                tween_update(&aq->aux_x, dt);
                tween_update(&aq->aux_y, dt);
            }
            if (aq->theseus_event_type == ANIM_EVT_TURNSTILE_ROTATE) {
                tween_update(&aq->rotation, dt);
            }
            if (aq->move_x.finished && aq->move_y.finished) {
                goto theseus_done;
            }
            break;
        }

        /* Normal hop or NONE */
        tween_update(&aq->move_x, dt);
        tween_update(&aq->move_y, dt);
        tween_update(&aq->hop, dt);
        if (aq->move_x.finished && aq->move_y.finished) {
            goto theseus_done;
        }
        break;

    theseus_done:
        {
            TurnResult res = aq->record.result;
            if (aq->record.minotaur_steps == 0 &&
                (res == TURN_RESULT_WIN || res == TURN_RESULT_LOSS_COLLISION)) {
                aq->playing = false;
                aq->phase = ANIM_PHASE_IDLE;
                return;
            }
        }
        if (!start_theseus_effects_phase(aq)) {
            TurnResult res = aq->record.result;
            if (aq->record.minotaur_steps == 0 &&
                res == TURN_RESULT_LOSS_HAZARD) {
                /* Hazard death during Theseus phase — still show env */
            }
            start_environment_phase(aq);
        }
        break;
    }

    case ANIM_PHASE_THESEUS_EFFECTS:
        tween_update(&aq->phase_tween, dt);
        if (aq->phase_tween.finished) {
            if (!advance_effect_event(aq)) {
                start_environment_phase(aq);
            }
        }
        break;

    case ANIM_PHASE_ENVIRONMENT:
        tween_update(&aq->phase_tween, dt);
        if (aq->effect_event_idx >= 0 &&
            aq->effect_event_idx < aq->record.event_count) {
            const AnimEvent* e = &aq->record.events[aq->effect_event_idx];
            if (e->type == ANIM_EVT_PLATFORM_MOVE ||
                e->type == ANIM_EVT_CONVEYOR_PUSH) {
                tween_update(&aq->aux_x, dt);
                tween_update(&aq->aux_y, dt);
            } else if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE) {
                tween_update(&aq->rotation, dt);
            }
        }
        if (aq->phase_tween.finished) {
            /* Lock in actor positions from completed event before advancing */
            update_env_tracking(aq);
            bool has_more = (aq->effect_event_idx >= 0) && advance_env_event(aq);
            if (!has_more) {
                TurnResult res = aq->record.result;
                if (aq->record.minotaur_steps == 0 &&
                    (res == TURN_RESULT_LOSS_HAZARD ||
                     res == TURN_RESULT_LOSS_COLLISION)) {
                    aq->playing = false;
                    aq->phase = ANIM_PHASE_IDLE;
                    return;
                }
                start_minotaur_step1(aq);
            }
        }
        break;

    case ANIM_PHASE_MINOTAUR_STEP1:
        tween_update(&aq->mino_x, dt);
        tween_update(&aq->mino_y, dt);
        if (aq->mino_x.finished && aq->mino_y.finished) {
            if (aq->record.minotaur_steps <= 1 &&
                aq->record.result == TURN_RESULT_LOSS_COLLISION) {
                aq->playing = false;
                aq->phase = ANIM_PHASE_IDLE;
                return;
            }
            start_minotaur_step2(aq);
        }
        break;

    case ANIM_PHASE_MINOTAUR_STEP2:
        tween_update(&aq->mino_x, dt);
        tween_update(&aq->mino_y, dt);
        if (aq->mino_x.finished && aq->mino_y.finished) {
            aq->playing = false;
            aq->phase = ANIM_PHASE_IDLE;
        }
        break;

    case ANIM_PHASE_IDLE:
        break;
    }
}

bool anim_queue_is_playing(const AnimQueue* aq) {
    return aq->playing;
}

AnimPhase anim_queue_phase(const AnimQueue* aq) {
    return aq->phase;
}

bool anim_queue_in_buffer_window(const AnimQueue* aq) {
    /* Buffer window is open during ANY animation phase (forward or reverse).
     * This lets the player queue their next action at any time while
     * animations are playing. Fast-forward kicks in as soon as input
     * is buffered, so remaining animations resolve quickly. */
    return aq->playing;
}

void anim_queue_theseus_pos(const AnimQueue* aq,
                            float* out_col, float* out_row,
                            float* out_hop) {
    if (aq->playing && aq->phase == ANIM_PHASE_THESEUS) {
        *out_col = tween_value(&aq->move_x);
        *out_row = tween_value(&aq->move_y);

        if (aq->theseus_sub == THESEUS_SUB_ICE_SLIDE ||
            aq->theseus_event_type == ANIM_EVT_THESEUS_TELEPORT ||
            aq->theseus_sub == THESEUS_SUB_PUSH) {
            *out_hop = 0.0f;
        } else {
            *out_hop = tween_value(&aq->hop) * ANIM_HOP_HEIGHT;
        }
    } else if (aq->playing && aq->phase == ANIM_PHASE_ENVIRONMENT) {
        /* During environment, Theseus might be moved by env effects.
         * Use env_theseus tracking position as default (updated
         * progressively as env events complete). */
        const AnimEvent* cur = anim_queue_current_event(aq);
        if (cur) {
            if (cur->type == ANIM_EVT_PLATFORM_MOVE && cur->platform.theseus_riding) {
                *out_col = tween_value(&aq->aux_x);
                *out_row = tween_value(&aq->aux_y);
                *out_hop = 0.0f;
                return;
            }
            if (cur->type == ANIM_EVT_CONVEYOR_PUSH &&
                cur->entity == ENTITY_THESEUS) {
                *out_col = tween_value(&aq->aux_x);
                *out_row = tween_value(&aq->aux_y);
                *out_hop = 0.0f;
                return;
            }
            if (cur->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE &&
                cur->turnstile.actor_moved[0]) {
                float t = tween_value(&aq->rotation);
                *out_col = (float)cur->turnstile.actor_from_col[0] +
                           ((float)cur->turnstile.actor_to_col[0] -
                            (float)cur->turnstile.actor_from_col[0]) * t;
                *out_row = (float)cur->turnstile.actor_from_row[0] +
                           ((float)cur->turnstile.actor_to_row[0] -
                            (float)cur->turnstile.actor_from_row[0]) * t;
                *out_hop = 0.0f;
                return;
            }
        }
        *out_col = aq->env_theseus_col;
        *out_row = aq->env_theseus_row;
        *out_hop = 0.0f;
    } else if (aq->playing && (aq->phase == ANIM_PHASE_MINOTAUR_STEP1 ||
                                aq->phase == ANIM_PHASE_MINOTAUR_STEP2)) {
        /* Post-environment position — env tracking has been finalized */
        *out_col = aq->env_theseus_col;
        *out_row = aq->env_theseus_row;
        *out_hop = 0.0f;
    } else {
        /* THESEUS_EFFECTS, IDLE, or not playing — pre-env position is correct */
        *out_col = (float)aq->record.theseus_to_col;
        *out_row = (float)aq->record.theseus_to_row;
        *out_hop = 0.0f;
    }
}

void anim_queue_minotaur_pos(const AnimQueue* aq,
                             float* out_col, float* out_row) {
    if (!aq->playing) {
        *out_col = (float)aq->record.minotaur_after2_col;
        *out_row = (float)aq->record.minotaur_after2_row;
        return;
    }

    switch (aq->phase) {
    case ANIM_PHASE_THESEUS:
    case ANIM_PHASE_THESEUS_EFFECTS:
        *out_col = (float)aq->record.minotaur_start_col;
        *out_row = (float)aq->record.minotaur_start_row;
        break;

    case ANIM_PHASE_ENVIRONMENT: {
        /* Use env_minotaur tracking position as default (updated
         * progressively as env events complete). */
        const AnimEvent* cur = anim_queue_current_event(aq);
        if (cur) {
            if (cur->type == ANIM_EVT_PLATFORM_MOVE && cur->platform.minotaur_riding) {
                *out_col = tween_value(&aq->aux_x);
                *out_row = tween_value(&aq->aux_y);
                return;
            }
            if (cur->type == ANIM_EVT_CONVEYOR_PUSH &&
                cur->entity == ENTITY_MINOTAUR) {
                *out_col = tween_value(&aq->aux_x);
                *out_row = tween_value(&aq->aux_y);
                return;
            }
            if (cur->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE &&
                cur->turnstile.actor_moved[1]) {
                float t = tween_value(&aq->rotation);
                *out_col = (float)cur->turnstile.actor_from_col[1] +
                           ((float)cur->turnstile.actor_to_col[1] -
                            (float)cur->turnstile.actor_from_col[1]) * t;
                *out_row = (float)cur->turnstile.actor_from_row[1] +
                           ((float)cur->turnstile.actor_to_row[1] -
                            (float)cur->turnstile.actor_from_row[1]) * t;
                return;
            }
        }
        *out_col = aq->env_minotaur_col;
        *out_row = aq->env_minotaur_row;
        break;
    }

    case ANIM_PHASE_MINOTAUR_STEP1:
    case ANIM_PHASE_MINOTAUR_STEP2:
        *out_col = tween_value(&aq->mino_x);
        *out_row = tween_value(&aq->mino_y);
        break;

    case ANIM_PHASE_IDLE:
        *out_col = (float)aq->record.minotaur_after2_col;
        *out_row = (float)aq->record.minotaur_after2_row;
        break;
    }
}

/* ── Query functions ─────────────────────────────────── */

AnimEventType anim_queue_theseus_event_type(const AnimQueue* aq) {
    return aq->theseus_event_type;
}

float anim_queue_teleport_progress(const AnimQueue* aq, int* out_phase) {
    if (aq->theseus_event_type != ANIM_EVT_THESEUS_TELEPORT ||
        aq->phase != ANIM_PHASE_THESEUS) {
        if (out_phase) *out_phase = -1;
        return -1.0f;
    }
    if (out_phase) {
        *out_phase = (aq->theseus_sub == THESEUS_SUB_TELEPORT_OUT) ? 0 : 1;
    }
    return tween_value(&aq->effect);
}

void anim_queue_aux_pos(const AnimQueue* aq,
                        float* out_col, float* out_row) {
    *out_col = tween_value(&aq->aux_x);
    *out_row = tween_value(&aq->aux_y);
}

float anim_queue_rotation_progress(const AnimQueue* aq) {
    return tween_value(&aq->rotation);
}

float anim_queue_effect_progress(const AnimQueue* aq) {
    return tween_value(&aq->phase_tween);
}

const AnimEvent* anim_queue_current_event(const AnimQueue* aq) {
    if (aq->effect_event_idx < 0 ||
        aq->effect_event_idx >= aq->record.event_count) {
        return NULL;
    }
    return &aq->record.events[aq->effect_event_idx];
}

bool anim_queue_is_ice_sliding(const AnimQueue* aq) {
    return (aq->playing &&
            aq->phase == ANIM_PHASE_THESEUS &&
            aq->theseus_event_type == ANIM_EVT_THESEUS_ICE_SLIDE &&
            aq->theseus_sub == THESEUS_SUB_ICE_SLIDE);
}

bool anim_queue_is_reversing(const AnimQueue* aq) {
    return aq->reversing;
}

void anim_queue_set_fast_forward(AnimQueue* aq, bool fast) {
    aq->fast_forward = fast;
}

void anim_queue_minotaur_dir(const AnimQueue* aq,
                              int* out_dir_col, int* out_dir_row) {
    *out_dir_col = aq->mino_dir_col;
    *out_dir_row = aq->mino_dir_row;
}

float anim_queue_minotaur_progress(const AnimQueue* aq) {
    if (aq->phase == ANIM_PHASE_MINOTAUR_STEP1 ||
        aq->phase == ANIM_PHASE_MINOTAUR_STEP2) {
        return tween_progress(&aq->mino_x);
    }
    return 0.0f;
}
