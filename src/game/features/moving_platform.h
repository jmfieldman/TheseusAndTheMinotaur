#ifndef GAME_FEATURE_MOVING_PLATFORM_H
#define GAME_FEATURE_MOVING_PLATFORM_H

#include "../feature.h"

struct cJSON;

/*
 * Moving Platform — floating tile over pit that follows a path.
 *
 * Config:
 *   "path":          array of {col, row} — movement path
 *   "mode":          "pingpong" or "loop"
 *   "initial_index": int — starting path index (default 0)
 *
 * Behaviour:
 *   - Platform moves one step along path each environment phase.
 *   - Actors on the platform ride with it.
 *   - Pit tiles without a platform are impassable to Minotaur and deadly to Theseus.
 *   - Pit tiles are marked impassable in the level JSON; the platform
 *     makes its current tile passable.
 */

Feature* moving_platform_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_MOVING_PLATFORM_H */
