#include "input/input_manager.h"
#include "input/keyboard_adapter.h"
#include "input/gamepad_adapter.h"
#include "engine/engine.h"
#include "engine/utils.h"

#include <SDL3/SDL.h>

#define ACTION_QUEUE_SIZE 16

static struct {
    InputContext    context;
    SemanticAction  queue[ACTION_QUEUE_SIZE];
    int             queue_head;
    int             queue_tail;
    bool            quit_requested;
} s_input = {
    .context = INPUT_CONTEXT_MENU,
    .queue_head = 0,
    .queue_tail = 0,
    .quit_requested = false,
};

static void enqueue_action(SemanticAction action) {
    if (action == ACTION_NONE) return;
    int next = (s_input.queue_tail + 1) % ACTION_QUEUE_SIZE;
    if (next == s_input.queue_head) {
        /* Queue full, drop oldest */
        s_input.queue_head = (s_input.queue_head + 1) % ACTION_QUEUE_SIZE;
    }
    s_input.queue[s_input.queue_tail] = action;
    s_input.queue_tail = next;
}

void input_manager_init(void) {
    s_input.context = INPUT_CONTEXT_MENU;
    s_input.queue_head = 0;
    s_input.queue_tail = 0;
    s_input.quit_requested = false;
    gamepad_adapter_init();
    LOG_INFO("Input manager initialized");
}

void input_manager_poll(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            s_input.quit_requested = true;
            g_engine.running = false;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                SemanticAction action = keyboard_adapter_map(
                    event.key.scancode, s_input.context);
                enqueue_action(action);
            }
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            {
                SemanticAction action = gamepad_adapter_map_button(
                    (SDL_GamepadButton)event.gbutton.button, s_input.context);
                enqueue_action(action);
            }
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
            gamepad_adapter_handle_added(event.gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            gamepad_adapter_handle_removed(event.gdevice.which);
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            g_engine.window_width  = event.window.data1;
            g_engine.window_height = event.window.data2;
            break;

        default:
            break;
        }
    }
}

SemanticAction input_manager_next_action(void) {
    if (s_input.queue_head == s_input.queue_tail) {
        return ACTION_NONE;
    }
    SemanticAction action = s_input.queue[s_input.queue_head];
    s_input.queue_head = (s_input.queue_head + 1) % ACTION_QUEUE_SIZE;
    return action;
}

void input_manager_set_context(InputContext context) {
    s_input.context = context;
}

InputContext input_manager_get_context(void) {
    return s_input.context;
}

bool input_manager_quit_requested(void) {
    return s_input.quit_requested;
}
