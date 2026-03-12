#include "input/gamepad_adapter.h"
#include "engine/utils.h"

#define MAX_GAMEPADS 4

static SDL_Gamepad* s_gamepads[MAX_GAMEPADS] = {0};

void gamepad_adapter_init(void) {
    /* Gamepads will be opened as they are connected via SDL events */
    LOG_INFO("Gamepad adapter initialized");
}

void gamepad_adapter_handle_added(SDL_JoystickID id) {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (s_gamepads[i] == NULL) {
            s_gamepads[i] = SDL_OpenGamepad(id);
            if (s_gamepads[i]) {
                LOG_INFO("Gamepad connected: %s (slot %d)",
                         SDL_GetGamepadName(s_gamepads[i]), i);
            }
            return;
        }
    }
    LOG_WARN("No free gamepad slot (max %d)", MAX_GAMEPADS);
}

void gamepad_adapter_handle_removed(SDL_JoystickID id) {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (s_gamepads[i] &&
            SDL_GetGamepadID(s_gamepads[i]) == id) {
            LOG_INFO("Gamepad disconnected (slot %d)", i);
            SDL_CloseGamepad(s_gamepads[i]);
            s_gamepads[i] = NULL;
            return;
        }
    }
}

SemanticAction gamepad_adapter_map_button(SDL_GamepadButton button, InputContext context) {
    switch (context) {
    case INPUT_CONTEXT_MENU:
        switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:    return ACTION_UI_UP;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return ACTION_UI_DOWN;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return ACTION_UI_LEFT;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ACTION_UI_RIGHT;
        case SDL_GAMEPAD_BUTTON_SOUTH:      return ACTION_UI_CONFIRM;
        case SDL_GAMEPAD_BUTTON_EAST:       return ACTION_UI_BACK;
        default: break;
        }
        break;

    case INPUT_CONTEXT_PUZZLE:
        switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:    return ACTION_MOVE_NORTH;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return ACTION_MOVE_SOUTH;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return ACTION_MOVE_WEST;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ACTION_MOVE_EAST;
        case SDL_GAMEPAD_BUTTON_SOUTH:      return ACTION_WAIT;
        case SDL_GAMEPAD_BUTTON_EAST:       return ACTION_UNDO;
        case SDL_GAMEPAD_BUTTON_NORTH:      return ACTION_RESET;
        case SDL_GAMEPAD_BUTTON_START:      return ACTION_PAUSE;
        default: break;
        }
        break;

    case INPUT_CONTEXT_OVERWORLD:
        switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:    return ACTION_OW_MOVE_NORTH;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return ACTION_OW_MOVE_SOUTH;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return ACTION_OW_MOVE_WEST;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ACTION_OW_MOVE_EAST;
        case SDL_GAMEPAD_BUTTON_SOUTH:      return ACTION_OW_ENTER;
        case SDL_GAMEPAD_BUTTON_EAST:       return ACTION_OW_BACK;
        default: break;
        }
        break;
    }

    return ACTION_NONE;
}

void gamepad_adapter_shutdown(void) {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (s_gamepads[i]) {
            SDL_CloseGamepad(s_gamepads[i]);
            s_gamepads[i] = NULL;
        }
    }
}
