#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "input/input_types.h"
#include <stdbool.h>

/* ---------- State interface (vtable) ---------- */

typedef struct State {
    /* Lifecycle */
    void (*on_enter)(struct State* self);
    void (*on_exit)(struct State* self);
    void (*on_pause)(struct State* self);   /* another state pushed on top */
    void (*on_resume)(struct State* self);  /* top state popped, this is active again */

    /* Per-frame */
    void (*handle_action)(struct State* self, SemanticAction action);
    void (*update)(struct State* self, float dt);
    void (*render)(struct State* self);

    /* Cleanup */
    void (*destroy)(struct State* self);

    /* Whether states below this one should also render (for overlays) */
    bool transparent;
} State;

/* ---------- State manager ---------- */

#define STATE_STACK_MAX 8

typedef struct {
    State* stack[STATE_STACK_MAX];
    int    top;  /* index of top state, -1 = empty */
} StateManager;

void  state_manager_init(StateManager* sm);
void  state_manager_shutdown(StateManager* sm);

/* Push a new state on top. Calls on_pause on current top, on_enter on new. */
void  state_manager_push(StateManager* sm, State* state);

/* Pop the top state. Calls on_exit + destroy on popped, on_resume on new top. */
void  state_manager_pop(StateManager* sm);

/* Replace the top state. Calls on_exit + destroy on old, on_enter on new. */
void  state_manager_swap(StateManager* sm, State* state);

/* Dispatch an action to the top state. */
void  state_manager_handle_action(StateManager* sm, SemanticAction action);

/* Update the top state. */
void  state_manager_update(StateManager* sm, float dt);

/* Render from bottom up (for transparency/overlay support). */
void  state_manager_render(StateManager* sm);

/* Get the current top state (or NULL). */
State* state_manager_top(StateManager* sm);

/* Is the stack empty? */
bool  state_manager_empty(StateManager* sm);

#endif /* STATE_MANAGER_H */
