#ifndef DIORAMA_GEN_H
#define DIORAMA_GEN_H

#include "render/voxel_mesh.h"
#include "render/lighting.h"
#include "data/biome_config.h"
#include "game/grid.h"

/*
 * Procedural diorama generator.
 *
 * Transforms a logical Grid + BiomeConfig into a rich 3D voxel scene
 * via a 12-step pipeline:
 *
 *   1. Platform base
 *   2. Floor tiles (paving stones with mortar gaps)
 *   3. Walls (stacked blocks with jitter)
 *   4. Back wall
 *   5. Doors (entrance/exit frames)
 *   6. Impassable cells
 *   7. Feature markers
 *   8. Floor decorations
 *   9. Wall decorations
 *  10. Lantern pillars + point lights
 *  11. Exit light
 *  12. Edge border
 *
 * Usage:
 *   BiomeConfig biome;
 *   biome_config_defaults(&biome);
 *   biome_config_load(&biome, "assets/biomes/stone_labyrinth.json");
 *
 *   VoxelMesh mesh;
 *   voxel_mesh_begin(&mesh);
 *
 *   DioramaGenResult result;
 *   diorama_generate(&mesh, grid, &biome, &result);
 *
 *   voxel_mesh_build(&mesh, 0.0625f);
 *
 *   // Apply point lights from result to your LightingState
 */

typedef struct {
    PointLight lights[LIGHTING_MAX_POINT_LIGHTS];
    int light_count;
    int grid_cols;    /* grid dimensions (for lightmap generation) */
    int grid_rows;
} DioramaGenResult;

/*
 * Generate the full diorama into the given VoxelMesh.
 *
 * The mesh must have been initialized with voxel_mesh_begin().
 * After this call, the caller should call voxel_mesh_build() to
 * finalize the geometry and bake AO.
 *
 * result: filled with point light positions/colors for lanterns and exit.
 */
void diorama_generate(VoxelMesh* mesh, const Grid* grid,
                      const BiomeConfig* biome, DioramaGenResult* result);

/*
 * Exclusion set for diorama generation — cells whose walls are skipped.
 * Used to separate auto-turnstile walls from the static mesh.
 */
typedef struct {
    int  cells[32][2];  /* (col, row) pairs */
    int  count;
} DioramaExcludeSet;

/*
 * Generate the full diorama but skip walls on excluded cells.
 * If exclude is NULL, behaves identically to diorama_generate().
 */
void diorama_generate_ex(VoxelMesh* mesh, const Grid* grid,
                          const BiomeConfig* biome, DioramaGenResult* result,
                          const DioramaExcludeSet* exclude);

/*
 * Generate ONLY the walls for a specific set of cells into a separate mesh.
 * The mesh must be initialized with voxel_mesh_begin().
 * Caller should call voxel_mesh_build() after this returns.
 */
void diorama_generate_walls_only(VoxelMesh* mesh, const Grid* grid,
                                  const BiomeConfig* biome,
                                  const int (*cells)[2], int cell_count);

#endif /* DIORAMA_GEN_H */
