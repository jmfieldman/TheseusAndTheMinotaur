#ifndef ENGINE_TWEEN_H
#define ENGINE_TWEEN_H

#include <stdbool.h>

/*
 * Tween — interpolates a single float value over time with easing.
 *
 * Usage:
 *   Tween tw;
 *   tween_init(&tw, 0.0f, 100.0f, 0.3f, EASE_LINEAR);
 *   // each frame:
 *   tween_update(&tw, dt);
 *   float val = tween_value(&tw);
 */

/* Easing function type — takes t in [0,1], returns eased t in [0,1] */
typedef float (*EaseFunc)(float t);

/* Built-in easing functions */
float ease_linear(float t);
float ease_in_quad(float t);
float ease_out_quad(float t);
float ease_in_out_quad(float t);
float ease_in_cubic(float t);
float ease_out_cubic(float t);
float ease_in_out_cubic(float t);
float ease_parabolic_arc(float t);   /* 0→1→0 arc (4t(1-t)), for hop height */
float ease_out_back(float t);        /* slight overshoot */

typedef struct {
    float   start;
    float   end;
    float   duration;
    float   elapsed;
    EaseFunc ease;
    bool    finished;
} Tween;

/* Initialize a tween. duration <= 0 means instant. */
void  tween_init(Tween* tw, float start, float end,
                 float duration, EaseFunc ease);

/* Advance the tween by dt seconds. */
void  tween_update(Tween* tw, float dt);

/* Get current interpolated value. */
float tween_value(const Tween* tw);

/* Get normalized progress [0,1]. */
float tween_progress(const Tween* tw);

/* Reset to beginning. */
void  tween_reset(Tween* tw);

#endif /* ENGINE_TWEEN_H */
