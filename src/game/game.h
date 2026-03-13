#ifndef GAME_GAME_H
#define GAME_GAME_H

/*
 * Game module — top-level convenience header.
 *
 * Includes the entire game logic API:
 *   - Grid and cell model
 *   - Feature vtable interface
 *   - Turn resolution
 *   - Minotaur AI
 *   - Undo system
 *   - Level loader
 *   - Feature registry
 */

#include "feature.h"
#include "grid.h"
#include "turn.h"
#include "minotaur.h"
#include "undo.h"
#include "level_loader.h"
#include "features/feature_registry.h"

#endif /* GAME_GAME_H */
