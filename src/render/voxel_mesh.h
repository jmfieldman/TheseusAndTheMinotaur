#ifndef VOXEL_MESH_H
#define VOXEL_MESH_H

#include <glad/gl.h>
#include <stdbool.h>

/*
 * Freeform box mesh builder — accumulates axis-aligned boxes at arbitrary
 * positions and dimensions into a single VBO.
 *
 * Vertex layout: position (vec3) + normal (vec3) + color (vec4) + uv (vec2)
 *   = 12 floats per vertex, 48 bytes.
 *
 * AO is computed via raytraced precomputed textures rather than per-vertex
 * baking. Each visible face gets an 8×8 texel tile in an AO texture atlas.
 * The shader samples the AO texture and multiplies it into the base color.
 *
 * Usage:
 *   VoxelMesh mesh;
 *   voxel_mesh_begin(&mesh);
 *   voxel_mesh_add_box(&mesh, pos, size, color, false);
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
 *      (unless the box is marked no_cull)
 *   3. Assigns UV coordinates mapping each face to an atlas tile
 *   4. Raycasts hemisphere AO per texel into the AO texture atlas
 *   5. Uploads geometry and AO texture to GPU; discards CPU-side data
 */

/* Maximum number of boxes that can be accumulated before build.
 * Sized for an 8×8 grid plus walls, impassable blocks, and decorations. */
#define VOXEL_MESH_MAX_BOXES 8192

/* AO texture tile size (texels per face side) */
#define AO_TILE_SIZE 8

typedef struct {
    float x, y, z;       /* position (min corner) */
    float sx, sy, sz;    /* size (extents) */
    float r, g, b, a;    /* color */
    bool  no_cull;       /* if true, all 6 faces are always emitted (for thin geometry like walls) */
    bool  occluder_only; /* if true, contributes to occupancy grid for AO but emits no faces */
} VoxelBox;

typedef struct {
    /* CPU-side staging (valid between begin and build) */
    VoxelBox boxes[VOXEL_MESH_MAX_BOXES];
    int      box_count;

    /* GPU resources (valid after build) */
    GLuint   vao;
    GLuint   vbo;
    GLuint   ao_texture;     /* AO texture atlas (R8, one channel) */
    int      ao_tex_width;
    int      ao_tex_height;
    int      vertex_count;
    bool     built;
    bool     has_ao;         /* true if AO texture was generated */
} VoxelMesh;

/* Initialize mesh for accumulating boxes. */
void voxel_mesh_begin(VoxelMesh* mesh);

/* Add an axis-aligned box with the given position, size, and color.
 * Position is the min corner (x,y,z). Size is the extent (sx,sy,sz).
 * If no_cull is true, all 6 faces are emitted regardless of occlusion
 * (use for thin geometry like walls that shouldn't self-occlude).
 * Any float coordinates and dimensions are accepted (not grid-snapped). */
void voxel_mesh_add_box(VoxelMesh* mesh,
                         float x, float y, float z,
                         float sx, float sy, float sz,
                         float r, float g, float b, float a,
                         bool no_cull);

/* Finalize the mesh: rasterize into occupancy grid, cull hidden faces,
 * raytrace AO into texture atlas, and upload to GPU.
 *
 * cell_size: occupancy grid cell size in world units. Smaller values give
 * more accurate culling/AO but use more memory. Recommended: tile_size/8.
 *
 * After this call, the CPU-side box list is cleared. */
void voxel_mesh_build(VoxelMesh* mesh, float cell_size);

/* Draw the built mesh. Caller must bind the voxel shader and set uniforms
 * (projection, view, model, lighting) before calling this.
 * If the mesh has an AO texture, it is bound to texture unit 0. */
void voxel_mesh_draw(const VoxelMesh* mesh);

/* Release GPU resources. Safe to call on unbuilt mesh. */
void voxel_mesh_destroy(VoxelMesh* mesh);

/* Query vertex count (for debugging/verification). */
int voxel_mesh_get_vertex_count(const VoxelMesh* mesh);

/* Query whether mesh has an AO texture. */
bool voxel_mesh_has_ao(const VoxelMesh* mesh);

/* Add an occluder-only box. Contributes to the occupancy grid for AO
 * raycasting but emits no visible faces. Use to simulate a ground plane
 * or other invisible geometry that should cast AO shadows. */
void voxel_mesh_add_occluder(VoxelMesh* mesh,
                              float x, float y, float z,
                              float sx, float sy, float sz);

#endif /* VOXEL_MESH_H */
