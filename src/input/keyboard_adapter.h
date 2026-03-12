#ifndef KEYBOARD_ADAPTER_H
#define KEYBOARD_ADAPTER_H

#include "input/input_types.h"
#include <SDL3/SDL.h>

/* Map an SDL keyboard event to a semantic action based on current context. */
SemanticAction keyboard_adapter_map(SDL_Scancode scancode, InputContext context);

#endif /* KEYBOARD_ADAPTER_H */
