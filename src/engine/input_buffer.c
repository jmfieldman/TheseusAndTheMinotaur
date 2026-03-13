#include "engine/input_buffer.h"
#include <SDL3/SDL.h>

void input_buffer_init(InputBuffer* buf) {
    buf->buffered    = ACTION_NONE;
    buf->window_open = false;
}

void input_buffer_open_window(InputBuffer* buf) {
    buf->window_open = true;
    buf->buffered    = ACTION_NONE;  /* clear any stale buffer */
}

void input_buffer_close_window(InputBuffer* buf) {
    buf->window_open = false;
}

bool input_buffer_is_bufferable(SemanticAction action) {
    switch (action) {
    case ACTION_MOVE_NORTH:
    case ACTION_MOVE_SOUTH:
    case ACTION_MOVE_EAST:
    case ACTION_MOVE_WEST:
    case ACTION_WAIT:
    case ACTION_UNDO:
    case ACTION_RESET:
        return true;
    default:
        return false;
    }
}

void input_buffer_accept(InputBuffer* buf, SemanticAction action) {
    if (!buf->window_open) return;
    if (!input_buffer_is_bufferable(action)) return;
    /* Last press wins */
    buf->buffered = action;
}

SemanticAction input_buffer_consume(InputBuffer* buf) {
    SemanticAction action = buf->buffered;
    buf->buffered = ACTION_NONE;
    return action;
}

bool input_buffer_window_is_open(const InputBuffer* buf) {
    return buf->window_open;
}

SemanticAction input_buffer_check_held_keys(void) {
    const bool* keys = SDL_GetKeyboardState(NULL);
    if (!keys) return ACTION_NONE;

    /*
     * Check direction keys (priority: arrow keys, then WASD).
     * Only return one action — first match wins.
     */
    if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) return ACTION_MOVE_NORTH;
    if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) return ACTION_MOVE_SOUTH;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) return ACTION_MOVE_WEST;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) return ACTION_MOVE_EAST;
    if (keys[SDL_SCANCODE_SPACE])                         return ACTION_WAIT;

    return ACTION_NONE;
}
