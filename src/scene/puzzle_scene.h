#ifndef PUZZLE_SCENE_H
#define PUZZLE_SCENE_H

#include "engine/state_manager.h"

/*
 * Puzzle scene — text-mode prototype.
 *
 * Renders the game grid as colored 2D rectangles:
 *   - Checkerboard floor tiles
 *   - Walls as thick colored lines on tile edges
 *   - Theseus as a blue square
 *   - Minotaur as a red square
 *   - Spike traps highlighted when active
 *   - Entrance/exit door markers
 *
 * Accepts puzzle input context actions (MOVE_*, WAIT, UNDO, RESET, PAUSE).
 * Resolves turns via turn_resolve().
 * Displays turn counter, win/loss text.
 *
 * This is a development prototype — will be replaced by the 3D diorama
 * renderer in a later step.
 */

/*
 * Create a puzzle scene that loads the given level JSON file.
 * The caller must push the returned State onto the engine state stack.
 * The puzzle scene owns the level_path string (copies it internally).
 */
State* puzzle_scene_create(const char* level_json_path);

#endif /* PUZZLE_SCENE_H */
