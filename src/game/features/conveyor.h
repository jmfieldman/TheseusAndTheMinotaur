#ifndef GAME_FEATURE_CONVEYOR_H
#define GAME_FEATURE_CONVEYOR_H

#include "../feature.h"

struct cJSON;

/*
 * Conveyor — directional tile that pushes actors during the environment phase.
 *
 * Config:
 *   "direction": "north" | "south" | "east" | "west"
 *
 * Behaviour:
 *   - During the environment phase, any actor (Theseus or Minotaur) standing
 *     on a conveyor tile is moved one step in the conveyor's direction.
 *   - If the destination is blocked (wall, impassable, or another actor),
 *     the actor stays put.
 *   - An actor pushed off a conveyor onto another conveyor is NOT moved
 *     again this turn (each conveyor fires once per environment phase).
 *   - No mutable state — purely positional.
 */

Feature* conveyor_create(int col, int row, const struct cJSON* config);

/*
 * Query the push direction of a conveyor feature.
 * Returns DIR_NONE if f is not a conveyor.
 */
Direction conveyor_get_direction(const Feature* f);

#endif /* GAME_FEATURE_CONVEYOR_H */
