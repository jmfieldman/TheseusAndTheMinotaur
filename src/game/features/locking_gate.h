#ifndef GAME_FEATURE_LOCKING_GATE_H
#define GAME_FEATURE_LOCKING_GATE_H

#include "../feature.h"

struct cJSON;

/*
 * Locking Gate — one-way passage that locks permanently after any actor
 * passes through.
 *
 * Config:
 *   "side":           string — the wall edge where the gate exists
 *   "passable_from":  string — direction from which the gate can be entered
 *
 * Behaviour:
 *   - Gate starts open — either actor can pass through.
 *   - When any actor passes through, gate locks (bars come up) permanently.
 *   - Locked gate acts as a wall for both actors.
 *
 * Implementation:
 *   The gate is placed on the cell at (col, row).  It monitors on_leave
 *   for that cell — when an actor leaves in the gate's direction, it sets
 *   the wall on that edge, locking the gate.
 */

Feature* locking_gate_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_LOCKING_GATE_H */
