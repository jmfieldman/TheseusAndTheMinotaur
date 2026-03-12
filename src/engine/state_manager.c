#include "engine/state_manager.h"
#include "engine/utils.h"

void state_manager_init(StateManager* sm) {
    sm->top = -1;
    memset(sm->stack, 0, sizeof(sm->stack));
}

void state_manager_shutdown(StateManager* sm) {
    /* Pop and destroy all states */
    while (sm->top >= 0) {
        State* s = sm->stack[sm->top];
        if (s->on_exit) s->on_exit(s);
        if (s->destroy) s->destroy(s);
        sm->stack[sm->top] = NULL;
        sm->top--;
    }
}

void state_manager_push(StateManager* sm, State* state) {
    if (sm->top >= STATE_STACK_MAX - 1) {
        LOG_ERROR("State stack overflow (max %d)", STATE_STACK_MAX);
        return;
    }

    /* Pause current top */
    if (sm->top >= 0 && sm->stack[sm->top]->on_pause) {
        sm->stack[sm->top]->on_pause(sm->stack[sm->top]);
    }

    sm->top++;
    sm->stack[sm->top] = state;

    if (state->on_enter) {
        state->on_enter(state);
    }

    LOG_DEBUG("State pushed (stack depth: %d)", sm->top + 1);
}

void state_manager_pop(StateManager* sm) {
    if (sm->top < 0) {
        LOG_WARN("Attempted to pop empty state stack");
        return;
    }

    State* old = sm->stack[sm->top];
    if (old->on_exit) old->on_exit(old);
    if (old->destroy) old->destroy(old);
    sm->stack[sm->top] = NULL;
    sm->top--;

    /* Resume new top */
    if (sm->top >= 0 && sm->stack[sm->top]->on_resume) {
        sm->stack[sm->top]->on_resume(sm->stack[sm->top]);
    }

    LOG_DEBUG("State popped (stack depth: %d)", sm->top + 1);
}

void state_manager_swap(StateManager* sm, State* state) {
    if (sm->top < 0) {
        /* Stack is empty, just push */
        state_manager_push(sm, state);
        return;
    }

    State* old = sm->stack[sm->top];
    if (old->on_exit) old->on_exit(old);
    if (old->destroy) old->destroy(old);

    sm->stack[sm->top] = state;

    if (state->on_enter) {
        state->on_enter(state);
    }

    LOG_DEBUG("State swapped (stack depth: %d)", sm->top + 1);
}

void state_manager_handle_action(StateManager* sm, SemanticAction action) {
    if (sm->top >= 0 && sm->stack[sm->top]->handle_action) {
        sm->stack[sm->top]->handle_action(sm->stack[sm->top], action);
    }
}

void state_manager_update(StateManager* sm, float dt) {
    if (sm->top >= 0 && sm->stack[sm->top]->update) {
        sm->stack[sm->top]->update(sm->stack[sm->top], dt);
    }
}

void state_manager_render(StateManager* sm) {
    if (sm->top < 0) return;

    /*
     * Find the lowest state that needs rendering.
     * Render bottom-up so overlays draw on top.
     */
    int bottom = sm->top;
    while (bottom > 0 && sm->stack[bottom]->transparent) {
        bottom--;
    }

    for (int i = bottom; i <= sm->top; i++) {
        if (sm->stack[i]->render) {
            sm->stack[i]->render(sm->stack[i]);
        }
    }
}

State* state_manager_top(StateManager* sm) {
    if (sm->top < 0) return NULL;
    return sm->stack[sm->top];
}

bool state_manager_empty(StateManager* sm) {
    return sm->top < 0;
}
