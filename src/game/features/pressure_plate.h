#ifndef GAME_FEATURE_PRESSURE_PLATE_H
#define GAME_FEATURE_PRESSURE_PLATE_H

#include "../feature.h"

struct cJSON;

/*
 * Pressure Plate — toggles linked walls/tile passability on Theseus step.
 *
 * Config:
 *   "color":   string — tint color identifier (e.g. "blue", "red")
 *   "targets": array of objects:
 *     - "type": "wall" or "tile"
 *     - "col", "row": position
 *     - "side": (walls only) which wall edge
 *     - "initial_active": bool — whether target starts active
 *
 * Behaviour:
 *   - When Theseus steps onto the plate, all linked targets toggle.
 *   - Toggling a wall: wall present ↔ wall absent.
 *   - Toggling a tile: impassable ↔ passable.
 *   - Minotaur does NOT trigger the plate.
 *   - Toggle is permanent until another plate or undo reverses it.
 */

Feature* pressure_plate_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_PRESSURE_PLATE_H */
