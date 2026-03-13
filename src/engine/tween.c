#include "engine/tween.h"

/* ── Easing functions ──────────────────────────────────── */

float ease_linear(float t) {
    return t;
}

float ease_in_quad(float t) {
    return t * t;
}

float ease_out_quad(float t) {
    return t * (2.0f - t);
}

float ease_in_out_quad(float t) {
    if (t < 0.5f) return 2.0f * t * t;
    return -1.0f + (4.0f - 2.0f * t) * t;
}

float ease_in_cubic(float t) {
    return t * t * t;
}

float ease_out_cubic(float t) {
    float u = t - 1.0f;
    return u * u * u + 1.0f;
}

float ease_in_out_cubic(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    float u = 2.0f * t - 2.0f;
    return 0.5f * u * u * u + 1.0f;
}

float ease_parabolic_arc(float t) {
    /* 0 at t=0, peaks at 1 at t=0.5, back to 0 at t=1 */
    return 4.0f * t * (1.0f - t);
}

float ease_out_back(float t) {
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

/* ── Tween operations ──────────────────────────────────── */

void tween_init(Tween* tw, float start, float end,
                float duration, EaseFunc ease) {
    tw->start    = start;
    tw->end      = end;
    tw->duration = duration > 0.0f ? duration : 0.0001f;
    tw->elapsed  = 0.0f;
    tw->ease     = ease ? ease : ease_linear;
    tw->finished = false;
}

void tween_update(Tween* tw, float dt) {
    if (tw->finished) return;
    tw->elapsed += dt;
    if (tw->elapsed >= tw->duration) {
        tw->elapsed  = tw->duration;
        tw->finished = true;
    }
}

float tween_value(const Tween* tw) {
    float t = tw->elapsed / tw->duration;
    if (t <= 0.0f) return tw->start;
    if (t >= 1.0f) return tw->end;
    float eased = tw->ease(t);
    return tw->start + (tw->end - tw->start) * eased;
}

float tween_progress(const Tween* tw) {
    float t = tw->elapsed / tw->duration;
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

void tween_reset(Tween* tw) {
    tw->elapsed  = 0.0f;
    tw->finished = false;
}
