#ifndef GAME_FEATURE_MEDUSA_WALL_H
#define GAME_FEATURE_MEDUSA_WALL_H

#include "../feature.h"

struct cJSON;

/*
 * Medusa Wall — wall-mounted face with line-of-sight kill.
 *
 * Config:
 *   "side":   string — which wall the Medusa is mounted on
 *   "facing": string — cardinal direction the Medusa faces (into grid)
 *
 * Behaviour:
 *   - If Theseus moves TOWARD the Medusa while in line of sight, he dies.
 *   - Moving away, perpendicular, or waiting are safe.
 *   - Walls block line of sight.
 *   - Minotaur is immune.
 */

Feature* medusa_wall_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_MEDUSA_WALL_H */
