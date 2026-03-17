#ifndef GAME_TILE_PHYSICS_H
#define GAME_TILE_PHYSICS_H

#include <stdbool.h>
#include "feature.h"
#include "grid.h"
#include "../data/biome_config.h"

/*
 * Tile physics query layer for death voxel scatter simulation.
 *
 * Provides physical surface properties of each tile (floor height, pit status,
 * passability) so that scattered death voxels interact correctly with the
 * environment: bouncing off walls, settling at correct floor heights, and
 * falling through bottomless pits.
 *
 * This is a pure query interface — it does not modify any game state.
 */

/* Physical properties of a tile for voxel physics. */
typedef struct {
    float surface_y;    /* Floor height in world space (0.0 = normal floor) */
    bool  is_pit;       /* Bottomless — voxels fall through and vanish */
    bool  is_impassable;/* Solid obstacle — voxels bounce off tile boundary */
} TileSurface;

/*
 * Query the physical surface properties of a tile.
 *
 * Accounts for all terrain types:
 *   Normal floor:               surface_y = 0.0
 *   Conveyor / auto-turnstile:  surface_y = 0.10 (elevated platform)
 *   Groove trench tiles:        surface_y = -trench_depth (recessed channel)
 *   Collapsed crumbling floor:  is_pit = true
 *   Moving-platform pit:        is_pit = true
 *   Impassable (solid block):   is_impassable = true
 *   Out-of-bounds:              is_pit = true
 */
TileSurface tile_physics_query(const Grid* grid,
                               const BiomeConfig* biome,
                               int col, int row);

/*
 * Check if a wall exists on the given side of (col, row) for voxel scatter.
 *
 * Returns true for actual grid walls AND for impassable neighbor tiles
 * (their boundary acts as a solid wall for voxel physics).
 */
bool tile_physics_has_wall(const Grid* grid,
                           int col, int row, Direction side);

/*
 * Get the world-space coordinate of a wall plane on the given side of a tile.
 *
 * For north/south walls: returns the Z coordinate of the wall surface.
 * For east/west walls:   returns the X coordinate of the wall surface.
 *
 * Accounts for WALL_THICKNESS (0.20) — returns the inner face of the wall
 * (the face that a voxel would bounce off of).
 */
float tile_physics_wall_coord(const Grid* grid,
                              int col, int row, Direction side);

#endif /* GAME_TILE_PHYSICS_H */
