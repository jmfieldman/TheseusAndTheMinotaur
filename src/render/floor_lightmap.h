#ifndef FLOOR_LIGHTMAP_H
#define FLOOR_LIGHTMAP_H

#include <glad/gl.h>
#include "render/voxel_mesh.h"
#include "data/biome_config.h"

/*
 * Floor shadow lightmap generator.
 *
 * Projects wall/obstacle footprints top-down onto a 2D lightmap texture,
 * applies Gaussian blur for soft edges, and adds per-tile surface effects
 * (edge darkening + grain noise).
 *
 * The resulting R8 texture is sampled by the voxel fragment shader for
 * floor faces tagged with AO_MODE_LIGHTMAP. This replaces the raytraced
 * AO baker for floor geometry, producing smoother, more tunable shadows
 * without the banding artifacts of discrete occupancy grid raycasting.
 *
 * Shadow parameters (scale, offset, blur, intensity) are configurable
 * per biome via FloorShadowConfig in the biome JSON.
 */

typedef struct {
    GLuint texture;   /* R8 texture (255=fully lit, 0=fully occluded) */
    int    width;     /* texture width in texels */
    int    height;    /* texture height in texels */
    float  origin_x;  /* world-space X origin of the lightmap */
    float  origin_z;  /* world-space Z origin of the lightmap */
    float  extent_x;  /* world-space X extent */
    float  extent_z;  /* world-space Z extent */
} FloorLightmap;

/*
 * Generate a floor shadow lightmap from the box list.
 *
 * Scans all boxes (skipping floor boxes with AO_MODE_LIGHTMAP) and projects
 * their XZ footprints as shadows, scaled/offset per FloorShadowConfig.
 * The result is blurred and uploaded as a GL texture.
 *
 * out:       receives the generated lightmap
 * boxes:     array of VoxelBox (from the mesh)
 * box_count: number of boxes
 * cols/rows: grid dimensions (defines lightmap world extent)
 * cfg:       per-biome shadow configuration
 */
void floor_lightmap_generate(FloorLightmap* out,
                              const VoxelBox* boxes, int box_count,
                              int cols, int rows,
                              const FloorShadowConfig* cfg);

/* Release GPU resources for the lightmap. */
void floor_lightmap_destroy(FloorLightmap* lm);

#endif /* FLOOR_LIGHTMAP_H */
