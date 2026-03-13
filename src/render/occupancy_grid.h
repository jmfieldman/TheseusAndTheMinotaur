#ifndef OCCUPANCY_GRID_H
#define OCCUPANCY_GRID_H

#include <stdbool.h>

/*
 * Coarse boolean occupancy grid used during voxel mesh build for:
 *   1. Hidden face culling — skip faces fully inside solid volume
 *   2. Baked ambient occlusion — sample neighboring cells per vertex
 *
 * The grid is allocated once during voxel_mesh_build(), populated by
 * rasterizing all placed boxes, queried per face/vertex, then freed.
 *
 * Resolution: ~8 subdivisions per game tile. For a 16×16 level this
 * gives 128×128 in XY. Z resolution is typically smaller (e.g. 32–64).
 *
 * Coordinates: world-space floats are mapped to grid cells via
 *   cell = floor((world - origin) / cell_size)
 */

typedef struct {
    bool*  cells;        /* 3D array [z * (sx*sy) + y * sx + x] */
    int    size_x;       /* grid dimensions */
    int    size_y;
    int    size_z;
    float  origin_x;     /* world-space origin of grid */
    float  origin_y;
    float  origin_z;
    float  cell_size;    /* world units per cell */
} OccupancyGrid;

/* Create a grid covering the given world-space AABB with the specified cell size. */
OccupancyGrid* occupancy_grid_create(float min_x, float min_y, float min_z,
                                      float max_x, float max_y, float max_z,
                                      float cell_size);

/* Free the grid. */
void occupancy_grid_destroy(OccupancyGrid* grid);

/* Mark all cells overlapping the given world-space box as occupied. */
void occupancy_grid_fill_box(OccupancyGrid* grid,
                              float x, float y, float z,
                              float sx, float sy, float sz);

/* Query whether a cell at grid indices (gx, gy, gz) is occupied.
 * Out-of-bounds queries return false (air). */
bool occupancy_grid_get(const OccupancyGrid* grid, int gx, int gy, int gz);

/* Query whether the cell containing world-space point (wx, wy, wz) is occupied. */
bool occupancy_grid_sample(const OccupancyGrid* grid, float wx, float wy, float wz);

/*
 * Compute baked ambient occlusion for a vertex at the given corner position.
 * Samples the 3 diagonal neighbors of the corner on the given face and returns
 * an AO multiplier in [0.0, 1.0] where 1.0 = fully lit, 0.0 = fully occluded.
 *
 * face_axis: 0=X, 1=Y, 2=Z
 * face_sign: +1 or -1 (which side of the face)
 * corner_u, corner_v: +1 or -1, indicating which corner of the face
 */
float occupancy_grid_vertex_ao(const OccupancyGrid* grid,
                                float wx, float wy, float wz,
                                int face_axis, int face_sign,
                                int corner_u, int corner_v);

#endif /* OCCUPANCY_GRID_H */
