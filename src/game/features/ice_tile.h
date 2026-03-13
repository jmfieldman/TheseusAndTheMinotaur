#ifndef GAME_FEATURE_ICE_TILE_H
#define GAME_FEATURE_ICE_TILE_H

#include "../feature.h"

struct cJSON;

/*
 * Ice Tile — slippery tile that causes Theseus to slide.
 *
 * Behaviour:
 *   - When Theseus steps onto ice, he slides until hitting a wall
 *     or reaching a non-ice tile.
 *   - Entire slide is one move (no env phase between steps).
 *   - Minotaur moves normally on ice.
 *   - No config needed beyond position.
 */

Feature* ice_tile_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_ICE_TILE_H */
