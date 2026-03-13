#ifndef GAME_FEATURE_CRUMBLING_FLOOR_H
#define GAME_FEATURE_CRUMBLING_FLOOR_H

#include "../feature.h"

struct cJSON;

/*
 * Crumbling Floor — passable once, collapses into deadly pit.
 *
 * Behaviour:
 *   - Starts intact (passable).
 *   - When any actor leaves the tile, it marks for collapse.
 *   - If Theseus waits on it, it also marks for collapse.
 *   - During environment phase, marked tiles collapse into pits
 *     (impassable + deadly to Theseus).
 *   - Minotaur is immune to collapse damage but pit is still impassable.
 */

Feature* crumbling_floor_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_CRUMBLING_FLOOR_H */
