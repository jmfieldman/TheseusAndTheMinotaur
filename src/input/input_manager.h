#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "input/input_types.h"
#include <stdbool.h>

/* Initialize the input manager and all adapters. */
void input_manager_init(void);

/* Process all pending SDL events, converting to semantic actions. */
void input_manager_poll(void);

/* Pop the next semantic action for this frame. Returns ACTION_NONE when empty. */
SemanticAction input_manager_next_action(void);

/* Set the current input context (determines action mapping). */
void input_manager_set_context(InputContext context);

/* Get the current input context. */
InputContext input_manager_get_context(void);

/* Returns true if the user requested window close (SDL_EVENT_QUIT). */
bool input_manager_quit_requested(void);

#endif /* INPUT_MANAGER_H */
