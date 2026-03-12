#include "input/keyboard_adapter.h"

/*
 * Default keyboard bindings. Maps SDL scancodes to semantic actions
 * based on the current input context.
 *
 * Design ref: 06-input.md §3.1
 */

SemanticAction keyboard_adapter_map(SDL_Scancode scancode, InputContext context) {
    switch (context) {
    case INPUT_CONTEXT_MENU:
        switch (scancode) {
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_W:     return ACTION_UI_UP;
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_S:     return ACTION_UI_DOWN;
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_A:     return ACTION_UI_LEFT;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_D:     return ACTION_UI_RIGHT;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_SPACE: return ACTION_UI_CONFIRM;
        case SDL_SCANCODE_ESCAPE:return ACTION_UI_BACK;
        default: break;
        }
        break;

    case INPUT_CONTEXT_PUZZLE:
        switch (scancode) {
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_W:     return ACTION_MOVE_NORTH;
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_S:     return ACTION_MOVE_SOUTH;
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_A:     return ACTION_MOVE_WEST;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_D:     return ACTION_MOVE_EAST;
        case SDL_SCANCODE_SPACE: return ACTION_WAIT;
        case SDL_SCANCODE_Z:     return ACTION_UNDO;
        case SDL_SCANCODE_R:     return ACTION_RESET;
        case SDL_SCANCODE_ESCAPE:return ACTION_PAUSE;
        default: break;
        }
        break;

    case INPUT_CONTEXT_OVERWORLD:
        switch (scancode) {
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_W:     return ACTION_OW_MOVE_NORTH;
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_S:     return ACTION_OW_MOVE_SOUTH;
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_A:     return ACTION_OW_MOVE_WEST;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_D:     return ACTION_OW_MOVE_EAST;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_SPACE: return ACTION_OW_ENTER;
        case SDL_SCANCODE_ESCAPE:return ACTION_OW_BACK;
        default: break;
        }
        break;
    }

    return ACTION_NONE;
}
