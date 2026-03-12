#ifndef GAMEPAD_ADAPTER_H
#define GAMEPAD_ADAPTER_H

#include "input/input_types.h"
#include <SDL3/SDL.h>

/* Initialize gamepad subsystem. */
void gamepad_adapter_init(void);

/* Handle a gamepad button event, return the mapped action. */
SemanticAction gamepad_adapter_map_button(SDL_GamepadButton button, InputContext context);

/* Handle gamepad connection/disconnection. */
void gamepad_adapter_handle_added(SDL_JoystickID id);
void gamepad_adapter_handle_removed(SDL_JoystickID id);

/* Shutdown and release gamepad resources. */
void gamepad_adapter_shutdown(void);

#endif /* GAMEPAD_ADAPTER_H */
