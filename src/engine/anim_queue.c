#include "engine/anim_queue.h"

/* ── Helpers ───────────────────────────────────────────── */

static void start_theseus_phase(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;

    aq->phase = ANIM_PHASE_THESEUS;

    if (r->theseus_moved) {
        tween_init(&aq->move_x,
                   (float)r->theseus_from_col,
                   (float)r->theseus_to_col,
                   ANIM_THESEUS_DURATION, ease_out_cubic);
        tween_init(&aq->move_y,
                   (float)r->theseus_from_row,
                   (float)r->theseus_to_row,
                   ANIM_THESEUS_DURATION, ease_out_cubic);
        /* Hop arc: 0 → peak → 0 */
        tween_init(&aq->hop, 0.0f, 1.0f,
                   ANIM_THESEUS_DURATION, ease_parabolic_arc);
    } else {
        /* No movement — zero-duration "animation" */
        tween_init(&aq->move_x,
                   (float)r->theseus_from_col,
                   (float)r->theseus_to_col,
                   0.001f, ease_linear);
        tween_init(&aq->move_y,
                   (float)r->theseus_from_row,
                   (float)r->theseus_to_row,
                   0.001f, ease_linear);
        tween_init(&aq->hop, 0.0f, 0.0f, 0.001f, ease_linear);
    }
}

static void start_environment_phase(AnimQueue* aq) {
    aq->phase = ANIM_PHASE_ENVIRONMENT;
    /* Brief pause so environment changes are visible */
    tween_init(&aq->move_x, 0.0f, 1.0f,
               ANIM_ENVIRONMENT_DURATION, ease_linear);
}

static void start_minotaur_step1(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;

    aq->phase = ANIM_PHASE_MINOTAUR_STEP1;

    if (r->minotaur_steps >= 1) {
        tween_init(&aq->mino_x,
                   (float)r->minotaur_start_col,
                   (float)r->minotaur_after1_col,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
        tween_init(&aq->mino_y,
                   (float)r->minotaur_start_row,
                   (float)r->minotaur_after1_row,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
    } else {
        /* Minotaur didn't move — skip instantly */
        tween_init(&aq->mino_x,
                   (float)r->minotaur_start_col,
                   (float)r->minotaur_start_col,
                   0.001f, ease_linear);
        tween_init(&aq->mino_y,
                   (float)r->minotaur_start_row,
                   (float)r->minotaur_start_row,
                   0.001f, ease_linear);
    }
}

static void start_minotaur_step2(AnimQueue* aq) {
    const TurnRecord* r = &aq->record;

    aq->phase = ANIM_PHASE_MINOTAUR_STEP2;

    if (r->minotaur_steps >= 2) {
        tween_init(&aq->mino_x,
                   (float)r->minotaur_after1_col,
                   (float)r->minotaur_after2_col,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
        tween_init(&aq->mino_y,
                   (float)r->minotaur_after1_row,
                   (float)r->minotaur_after2_row,
                   ANIM_MINOTAUR_DURATION, ease_in_out_quad);
    } else {
        /* Minotaur didn't take step 2 — skip instantly */
        tween_init(&aq->mino_x,
                   (float)r->minotaur_after1_col,
                   (float)r->minotaur_after1_col,
                   0.001f, ease_linear);
        tween_init(&aq->mino_y,
                   (float)r->minotaur_after1_row,
                   (float)r->minotaur_after1_row,
                   0.001f, ease_linear);
    }
}

/* ── Public API ────────────────────────────────────────── */

void anim_queue_init(AnimQueue* aq) {
    aq->phase   = ANIM_PHASE_IDLE;
    aq->playing = false;
}

void anim_queue_start(AnimQueue* aq, const TurnRecord* record) {
    aq->record  = *record;
    aq->playing = true;

    /* Begin with Theseus phase */
    start_theseus_phase(aq);
}

void anim_queue_update(AnimQueue* aq, float dt) {
    if (!aq->playing) return;

    switch (aq->phase) {
    case ANIM_PHASE_THESEUS:
        tween_update(&aq->move_x, dt);
        tween_update(&aq->move_y, dt);
        tween_update(&aq->hop, dt);
        if (aq->move_x.finished && aq->move_y.finished) {
            /* Early termination: if result was determined during Theseus
             * phase (win, loss from walking into minotaur/hazard), or
             * if it was a push (no env/mino needed for visual), skip
             * to environment. */
            TurnResult res = aq->record.result;
            if (res == TURN_RESULT_WIN ||
                res == TURN_RESULT_LOSS_COLLISION ||
                res == TURN_RESULT_LOSS_HAZARD) {
                /* Check if loss/win happened during Theseus phase
                 * (minotaur_steps == 0 means no minotaur phase ran) */
                if (aq->record.minotaur_steps == 0 &&
                    (res == TURN_RESULT_WIN ||
                     res == TURN_RESULT_LOSS_COLLISION ||
                     res == TURN_RESULT_LOSS_HAZARD)) {
                    /* Still run environment visual pause for hazard deaths
                     * that occur during env phase. For collision/win during
                     * Theseus phase, skip env too. */
                    if (res == TURN_RESULT_WIN ||
                        res == TURN_RESULT_LOSS_COLLISION) {
                        /* Death/win during Theseus phase — done */
                        aq->playing = false;
                        aq->phase   = ANIM_PHASE_IDLE;
                        return;
                    }
                }
            }
            start_environment_phase(aq);
        }
        break;

    case ANIM_PHASE_ENVIRONMENT:
        tween_update(&aq->move_x, dt);
        if (aq->move_x.finished) {
            TurnResult res = aq->record.result;
            /* If env phase caused death, end here */
            if (aq->record.minotaur_steps == 0 &&
                (res == TURN_RESULT_LOSS_HAZARD ||
                 res == TURN_RESULT_LOSS_COLLISION)) {
                aq->playing = false;
                aq->phase   = ANIM_PHASE_IDLE;
                return;
            }
            start_minotaur_step1(aq);
        }
        break;

    case ANIM_PHASE_MINOTAUR_STEP1:
        tween_update(&aq->mino_x, dt);
        tween_update(&aq->mino_y, dt);
        if (aq->mino_x.finished && aq->mino_y.finished) {
            /* If collision after step 1 and no step 2, end */
            if (aq->record.minotaur_steps <= 1 &&
                aq->record.result == TURN_RESULT_LOSS_COLLISION) {
                aq->playing = false;
                aq->phase   = ANIM_PHASE_IDLE;
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
            aq->phase   = ANIM_PHASE_IDLE;
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
    if (!aq->playing) return false;

    const TurnRecord* r = &aq->record;

    /*
     * Buffer window opens during the Minotaur's LAST step:
     * - 2 steps: window during step 2
     * - 1 step:  window during step 1 (it is the last step)
     * - 0 steps: no window
     */
    if (r->minotaur_steps == 0) return false;

    if (r->minotaur_steps == 2) {
        return aq->phase == ANIM_PHASE_MINOTAUR_STEP2;
    }

    /* 1 step: step 1 is the last step */
    return aq->phase == ANIM_PHASE_MINOTAUR_STEP1;
}

void anim_queue_theseus_pos(const AnimQueue* aq,
                            float* out_col, float* out_row,
                            float* out_hop) {
    if (aq->playing && aq->phase == ANIM_PHASE_THESEUS) {
        *out_col = tween_value(&aq->move_x);
        *out_row = tween_value(&aq->move_y);
        *out_hop = tween_value(&aq->hop) * ANIM_HOP_HEIGHT;
    } else {
        /* After Theseus phase, use final position */
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
    case ANIM_PHASE_ENVIRONMENT:
        /* Minotaur hasn't moved yet */
        *out_col = (float)aq->record.minotaur_start_col;
        *out_row = (float)aq->record.minotaur_start_row;
        break;

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
