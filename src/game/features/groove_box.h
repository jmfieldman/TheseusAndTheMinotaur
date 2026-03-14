#ifndef GAME_FEATURE_GROOVE_BOX_H
#define GAME_FEATURE_GROOVE_BOX_H

#include "../feature.h"

struct cJSON;

/*
 * Groove Box — pushable box constrained to a groove track.
 *
 * Config:
 *   "groove":      array of {col, row} — the track tiles
 *   "initial_pos": {col, row} — box starting position (must be on groove)
 *
 * Behaviour:
 *   - Theseus can push the box along the groove (aligned direction only).
 *   - Push costs Theseus's move action. Theseus enters vacated tile.
 *   - Box blocks both actors. Minotaur cannot push.
 *   - Box cannot be pushed off-groove, into walls, or into other boxes.
 */

Feature* groove_box_create(int col, int row, const struct cJSON* config);

/*
 * Query the groove path tiles for diorama generation.
 *
 * Returns the number of path tiles written (0 if f is not a groove_box).
 * out_cols/out_rows are filled with the tile coordinates.
 * max_out limits the number of entries written.
 */
int groove_box_get_path(const Feature* f, int* out_cols, int* out_rows, int max_out);

#endif /* GAME_FEATURE_GROOVE_BOX_H */
