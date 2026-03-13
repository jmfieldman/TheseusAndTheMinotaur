#ifndef GAME_MINOTAUR_H
#define GAME_MINOTAUR_H

#include "grid.h"

/*
 * Minotaur AI — greedy chase with horizontal priority.
 *
 * Per design doc:
 *   1. The Minotaur gets 2 steps per turn.
 *   2. Each step, it first tries to close horizontal distance to Theseus.
 *      If that's blocked (wall/impassable/feature), it tries vertical.
 *      If both are blocked, the step is forfeited.
 *   3. Horizontal priority: if the Minotaur is east of Theseus, it tries
 *      to move west first; if west, it moves east; if same column, skip
 *      horizontal and try vertical.
 *   4. The Minotaur is immune to hazards but IS blocked by walls and
 *      impassable tiles.
 *   5. The Minotaur cannot exit through the exit door.
 *
 * Returns the number of steps actually taken (0, 1, or 2).
 * After each step, the caller should check for collision (loss condition).
 */
int minotaur_take_turn(Grid* grid);

/*
 * Execute a single Minotaur step (for fine-grained control).
 * Returns true if the Minotaur moved, false if both directions blocked.
 */
bool minotaur_step(Grid* grid);

#endif /* GAME_MINOTAUR_H */
