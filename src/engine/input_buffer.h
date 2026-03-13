#ifndef ENGINE_INPUT_BUFFER_H
#define ENGINE_INPUT_BUFFER_H

#include "input/input_types.h"
#include <stdbool.h>

/*
 * InputBuffer — single-slot action buffer for input during animations.
 *
 * Design doc §10.2–10.4:
 *   - Buffer window opens during the Minotaur's last step animation
 *   - Only fresh key-down events accepted (held keys ignored)
 *   - Last press wins if multiple presses during window
 *   - Bufferable: Move, Wait, Undo, Reset (not Pause)
 *   - On animation complete: if buffered, resolve immediately;
 *     else check held keys, else wait for input
 */

typedef struct {
    SemanticAction  buffered;       /* ACTION_NONE if empty */
    bool            window_open;    /* is the buffer window active? */
} InputBuffer;

/* Initialize (empty, window closed). */
void input_buffer_init(InputBuffer* buf);

/* Open the buffer window. Call when minotaur's last step starts. */
void input_buffer_open_window(InputBuffer* buf);

/* Close the buffer window. Call when animation ends. */
void input_buffer_close_window(InputBuffer* buf);

/*
 * Offer an action to the buffer.
 * Only accepted if:
 *   - Window is open
 *   - Action is bufferable (Move, Wait, Undo, Reset)
 * Last press wins.
 */
void input_buffer_accept(InputBuffer* buf, SemanticAction action);

/*
 * Consume and return the buffered action, clearing the buffer.
 * Returns ACTION_NONE if nothing buffered.
 */
SemanticAction input_buffer_consume(InputBuffer* buf);

/* Check if an action is bufferable (Move, Wait, Undo, Reset). */
bool input_buffer_is_bufferable(SemanticAction action);

/* Is the window currently open? */
bool input_buffer_window_is_open(const InputBuffer* buf);

/*
 * Check currently-held keys and return the corresponding puzzle action.
 * Uses SDL_GetKeyboardState() to detect held direction/wait keys.
 * Returns ACTION_NONE if no relevant key is held.
 *
 * Called on animation complete when no buffered action exists,
 * per design doc §10.4 item 2.
 */
SemanticAction input_buffer_check_held_keys(void);

#endif /* ENGINE_INPUT_BUFFER_H */
