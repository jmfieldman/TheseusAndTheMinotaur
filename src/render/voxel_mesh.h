#ifndef VOXEL_MESH_H
#define VOXEL_MESH_H

#include <glad/gl.h>
#include <stdbool.h>

/*
 * Freeform box mesh builder — accumulates axis-aligned boxes at arbitrary
 * positions and dimensions into a single VBO.
 *
 * Vertex layout: position (vec3) + normal (vec3) + color (vec4) + uv (vec2)
 *   + ao_mode (float) = 13 floats per vertex, 52 bytes.
 *
 * Three-tier AO system:
 *   AO_MODE_NONE (0)     — wall heuristic: darkening baked into vertex color
 *   AO_MODE_ATLAS (1)    — raytraced AO texture atlas (complex geometry)
 *   AO_MODE_LIGHTMAP (2) — floor shadow lightmap (smooth, tunable)
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

/* AO texture tile size (texels per face side).
 * Higher values give smoother edge/grain effects but use more atlas space.
 * 32 texels per face side gives good quality at reasonable cost. */
#define AO_TILE_SIZE 32

/* AO mode for each box — controls how ambient occlusion is computed. */
typedef enum {
    AO_MODE_NONE     = 0,  /* Wall heuristic: darkening baked into vertex color */
    AO_MODE_ATLAS    = 1,  /* Raytraced AO texture atlas (complex geometry) */
    AO_MODE_LIGHTMAP = 2,  /* Floor shadow lightmap (smooth, tunable) */
    AO_MODE_SHADOW   = 3,  /* Actor shadow: samples shadow texture as alpha */
} AoMode;

/* Wall orientation — controls which axis the slab pattern divides along.
 * Only meaningful when ao_mode == AO_MODE_NONE. Passed to the shader
 * via the UV.x channel. */
typedef enum {
    WALL_ORIENT_H      = 0,  /* Horizontal wall: slabs divide along X */
    WALL_ORIENT_V      = 1,  /* Vertical wall: slabs divide along Z */
    WALL_ORIENT_CORNER = 2,  /* Corner block: slab edges on all sides */
} WallOrient;

typedef struct {
    float x, y, z;       /* position (min corner) */
    float sx, sy, sz;    /* size (extents) */
    float r, g, b, a;    /* color */
    bool  no_cull;       /* if true, all 6 faces are always emitted (for thin geometry like walls) */
    bool  occluder_only; /* if true, contributes to occupancy grid for AO but emits no faces */
    uint8_t ao_mode;     /* AoMode — controls per-face AO routing (default AO_MODE_ATLAS) */
    uint8_t wall_orient; /* WallOrient — slab axis for AO_MODE_NONE walls (default WALL_ORIENT_H) */
    uint8_t subdivisions; /* face subdivision level: 1=default, N=N×N quads per face */
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

    /* Universal shadow softness (0=hard, 1=soft). Set before build().
     * Controls wall heuristic gradient widths and darkening amounts.
     * See FloorShadowConfig in biome_config.h for documentation. */
    float    shadow_softness;

    /* Current subdivision level for newly added boxes (1=default). */
    int      cur_subdivisions;

    /* Floor lightmap (set before build, used by lightmap-mode faces) */
    GLuint   floor_lm_texture;  /* R8 texture handle (0 if none) */
    float    floor_lm_origin_x; /* world-space lightmap origin X */
    float    floor_lm_origin_z; /* world-space lightmap origin Z */
    float    floor_lm_extent_x; /* world-space lightmap extent X */
    float    floor_lm_extent_z; /* world-space lightmap extent Z */
    int      floor_lm_cols;     /* grid cols for UV computation */
    int      floor_lm_rows;     /* grid rows for UV computation */
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

/* Add a box with explicit AO mode. This is the extended version of
 * voxel_mesh_add_box() that allows specifying how AO should be computed
 * for this box's faces. */
void voxel_mesh_add_box_ex(VoxelMesh* mesh,
                            float x, float y, float z,
                            float sx, float sy, float sz,
                            float r, float g, float b, float a,
                            bool no_cull, AoMode ao_mode);

/* Add a wall box with explicit AO mode and wall orientation.
 * wall_orient controls which axis the procedural slab pattern divides along. */
void voxel_mesh_add_wall(VoxelMesh* mesh,
                          float x, float y, float z,
                          float sx, float sy, float sz,
                          float r, float g, float b, float a,
                          bool no_cull, WallOrient orient);

/* Add an occluder-only box. Contributes to the occupancy grid for AO
 * raycasting but emits no visible faces. Use to simulate a ground plane
 * or other invisible geometry that should cast AO shadows. */
void voxel_mesh_add_occluder(VoxelMesh* mesh,
                              float x, float y, float z,
                              float sx, float sy, float sz);

/* Set the subdivision level for subsequently added boxes.
 * N=1 (default) emits the standard 2-triangle face. N>1 subdivides each
 * face into an N×N grid of quads (6*N*N vertices per face) for smooth
 * vertex-shader deformations. Only useful for actor meshes. */
void voxel_mesh_set_subdivisions(VoxelMesh* mesh, int subdivs);

/* Set the floor lightmap texture for lightmap-mode faces.
 * Must be called before voxel_mesh_build(). The mesh takes ownership
 * of the texture handle (will delete it in voxel_mesh_destroy). */
void voxel_mesh_set_floor_lightmap(VoxelMesh* mesh, GLuint texture,
                                    float origin_x, float origin_z,
                                    float extent_x, float extent_z,
                                    int cols, int rows);

#endif /* VOXEL_MESH_H */
