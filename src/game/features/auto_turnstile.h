#ifndef GAME_FEATURE_AUTO_TURNSTILE_H
#define GAME_FEATURE_AUTO_TURNSTILE_H

#include "../feature.h"

struct cJSON;

/*
 * Auto-Turnstile — rotates 4 tiles at a junction every environment phase.
 *
 * Config:
 *   "junction_col", "junction_row": the corner point
 *   "direction": "cw" or "ccw"
 *
 * The junction is the corner shared by tiles:
 *   (jc-1, jr), (jc, jr), (jc, jr-1), (jc-1, jr-1)
 *
 * Each env phase: walls, actors, and features on those 4 tiles rotate 90°.
 */

Feature* auto_turnstile_create(int col, int row, const struct cJSON* config);

/*
 * Query junction info from an auto-turnstile feature.
 * Returns true if the feature is an auto_turnstile.
 */
bool auto_turnstile_get_junction(const Feature* f,
                                  int* out_jc, int* out_jr,
                                  bool* out_clockwise);

#endif /* GAME_FEATURE_AUTO_TURNSTILE_H */
