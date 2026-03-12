#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "engine/state_manager.h"
#include "input/input_types.h"

/* ---------- Engine ---------- */

typedef struct {
    SDL_Window*     window;
    SDL_GLContext   gl_context;
    bool            running;

    int             window_width;
    int             window_height;

    /* Fixed timestep */
    float           target_dt;         /* 1/60 */
    uint64_t        last_tick;
    float           accumulator;

    /* State manager */
    StateManager    state_manager;
} Engine;

/* Global engine instance */
extern Engine g_engine;

/* Lifecycle */
bool engine_init(const char* title, int width, int height);
void engine_run(void);
void engine_shutdown(void);
void engine_quit(void);

/* State management shortcuts */
void engine_push_state(State* state);
void engine_pop_state(void);
void engine_swap_state(State* state);

#endif /* ENGINE_H */
