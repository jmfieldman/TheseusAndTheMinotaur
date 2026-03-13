#ifndef VOXEL_MESH_H
#define VOXEL_MESH_H

#include <glad/gl.h>
#include <stdbool.h>

/*
 * Freeform box mesh builder — accumulates axis-aligned boxes at arbitrary
 * positions and dimensions into a single VBO.
 *
 * Vertex layout: position (vec3) + normal (vec3) + color (vec4, RGBA with AO baked in)
 *   = 10 floats per vertex, 40 bytes.
 *
 * Usage:
 *   VoxelMesh mesh;
 *   voxel_mesh_begin(&mesh);
 *   voxel_mesh_add_box(&mesh, pos, size, color);
 *   voxel_mesh_add_box(&mesh, ...);
 *   voxel_mesh_build(&mesh, cell_size);   // rasterize → cull → AO → upload
 *   // ... render loop ...
 *   voxel_mesh_draw(&mesh);
 *   // ... cleanup ...
 *   voxel_mesh_destroy(&mesh);
 *
 * During build():
 *   1. Rasterizes all boxes into a coarse occupancy grid
 *   2. Emits vertices for each face, skipping fully occluded faces
 *   3. Computes baked AO per vertex by sampling neighboring occupancy
 *   4. Uploads to GPU; discards CPU-side data and occupancy grid
 */

/* Maximum number of boxes that can be accumulated before build. */
#define VOXEL_MESH_MAX_BOXES 4096

typedef struct {
    float x, y, z;       /* position (min corner) */
    float sx, sy, sz;    /* size (extents) */
    float r, g, b, a;    /* color */
} VoxelBox;

typedef struct {
    /* CPU-side staging (valid between begin and build) */
    VoxelBox boxes[VOXEL_MESH_MAX_BOXES];
    int      box_count;

    /* GPU resources (valid after build) */
    GLuint   vao;
    GLuint   vbo;
    int      vertex_count;
    bool     built;
} VoxelMesh;

/* Initialize mesh for accumulating boxes. */
void voxel_mesh_begin(VoxelMesh* mesh);

/* Add an axis-aligned box with the given position, size, and color.
 * Position is the min corner (x,y,z). Size is the extent (sx,sy,sz).
 * Any float coordinates and dimensions are accepted (not grid-snapped). */
void voxel_mesh_add_box(VoxelMesh* mesh,
                         float x, float y, float z,
                         float sx, float sy, float sz,
                         float r, float g, float b, float a);

/* Finalize the mesh: rasterize into occupancy grid, cull hidden faces,
 * compute baked AO, and upload to GPU.
 *
 * cell_size: occupancy grid cell size in world units. Smaller values give
 * more accurate culling/AO but use more memory. Recommended: tile_size/8.
 *
 * After this call, the CPU-side box list is cleared. */
void voxel_mesh_build(VoxelMesh* mesh, float cell_size);

/* Draw the built mesh. Caller must bind the voxel shader and set uniforms
 * (projection, view, model, lighting) before calling this. */
void voxel_mesh_draw(const VoxelMesh* mesh);

/* Release GPU resources. Safe to call on unbuilt mesh. */
void voxel_mesh_destroy(VoxelMesh* mesh);

/* Query vertex count (for debugging/verification). */
int voxel_mesh_get_vertex_count(const VoxelMesh* mesh);

#endif /* VOXEL_MESH_H */
