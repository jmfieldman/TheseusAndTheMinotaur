#ifndef GAME_FEATURE_MANUAL_TURNSTILE_H
#define GAME_FEATURE_MANUAL_TURNSTILE_H

#include "../feature.h"

struct cJSON;

/*
 * Manual Turnstile — player pushes to rotate walls at a junction.
 *
 * Config:
 *   "junction_col", "junction_row": the corner point
 *
 * Behaviour:
 *   - When Theseus pushes against a wall at the junction, all walls
 *     connected to that junction rotate 90° in the push direction.
 *   - Theseus moves with the rotation (e.g. pushing from SW northward
 *     rotates CW and moves Theseus to NW).
 *   - Minotaur cannot activate.
 */

Feature* manual_turnstile_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_MANUAL_TURNSTILE_H */
