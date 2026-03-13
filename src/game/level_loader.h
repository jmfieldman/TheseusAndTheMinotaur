#ifndef GAME_LEVEL_LOADER_H
#define GAME_LEVEL_LOADER_H

#include "grid.h"

/*
 * Level loader — parses a level JSON file into a Grid.
 *
 * Expected JSON format (from design doc §09):
 *
 *   {
 *     "id": "forest-03",
 *     "name": "Tangled Path",
 *     "biome": "dark_forest",
 *     "difficulty": 3,
 *     "optimal_turns": 14,
 *     "grid": { "width": 6, "height": 8 },
 *     "entrance": { "col": 0, "row": 3, "side": "west" },
 *     "exit":     { "col": 5, "row": 4, "side": "east" },
 *     "theseus":  { "col": 0, "row": 3 },
 *     "minotaur": { "col": 5, "row": 7 },
 *     "walls": [
 *       { "col": 2, "row": 3, "side": "east" },
 *       ...
 *     ],
 *     "impassable": [
 *       { "col": 4, "row": 5 },
 *       ...
 *     ],
 *     "features": [
 *       { "type": "spike_trap", "position": { "col": 4, "row": 2 },
 *         "config": { "cycle": 3, "initial_phase": 0 } },
 *       ...
 *     ]
 *   }
 *
 * Features in the "features" array are instantiated via a registry
 * (see level_loader_register_feature_factory).
 */

/*
 * Feature factory function type.
 *
 * Called when the loader encounters a feature in the JSON.
 * The factory should allocate a Feature, set its vtable and data
 * from the config JSON object, and return it.
 *
 * config_json is the cJSON object for the "config" field (may be NULL).
 * col, row is the feature's grid position.
 *
 * Return NULL if the feature type is unrecognised or invalid.
 */
struct cJSON;  /* forward declaration to avoid cJSON include in header */
typedef Feature* (*FeatureFactory)(int col, int row,
                                   const struct cJSON* config_json);

/*
 * Register a feature factory for a given type name.
 * Call this at startup before loading any levels.
 *
 * Example:
 *   level_loader_register_feature("spike_trap", spike_trap_create);
 */
void level_loader_register_feature(const char* type_name,
                                   FeatureFactory factory);

/*
 * Load a level from a JSON file on disk.
 * Returns a fully-initialized Grid, or NULL on failure.
 * The caller owns the returned Grid and must call grid_destroy().
 */
Grid* level_load_from_file(const char* json_path);

/*
 * Load a level from a JSON string in memory.
 * Returns a fully-initialized Grid, or NULL on failure.
 */
Grid* level_load_from_string(const char* json_str);

#endif /* GAME_LEVEL_LOADER_H */
