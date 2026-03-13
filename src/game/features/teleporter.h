#ifndef GAME_FEATURE_TELEPORTER_H
#define GAME_FEATURE_TELEPORTER_H

#include "../feature.h"

struct cJSON;

/*
 * Teleporter Pair — step on one, instantly appear at the other.
 *
 * Config:
 *   "pair_id": string — shared identifier linking two teleporter tiles
 *
 * Behaviour:
 *   - Both Theseus and Minotaur trigger teleportation.
 *   - No chaining (arriving at destination does not re-trigger).
 *   - If destination is occupied by other actor, collision applies.
 */

Feature* teleporter_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_TELEPORTER_H */
