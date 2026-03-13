#include "render/occupancy_grid.h"
#include "engine/utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- Helpers ---------- */

static inline int world_to_cell(float world, float origin, float cell_size) {
    return (int)floorf((world - origin) / cell_size);
}

static inline int grid_index(const OccupancyGrid* g, int x, int y, int z) {
    return z * (g->size_x * g->size_y) + y * g->size_x + x;
}

static inline bool in_bounds(const OccupancyGrid* g, int x, int y, int z) {
    return x >= 0 && x < g->size_x &&
           y >= 0 && y < g->size_y &&
           z >= 0 && z < g->size_z;
}

/* ---------- Create / Destroy ---------- */

OccupancyGrid* occupancy_grid_create(float min_x, float min_y, float min_z,
                                      float max_x, float max_y, float max_z,
                                      float cell_size) {
    OccupancyGrid* g = (OccupancyGrid*)calloc(1, sizeof(OccupancyGrid));
    if (!g) return NULL;

    /* Add a 2-cell margin on each side for AO sampling */
    float margin = cell_size * 2.0f;
    g->origin_x = min_x - margin;
    g->origin_y = min_y - margin;
    g->origin_z = min_z - margin;
    g->cell_size = cell_size;

    g->size_x = (int)ceilf((max_x + margin - g->origin_x) / cell_size) + 1;
    g->size_y = (int)ceilf((max_y + margin - g->origin_y) / cell_size) + 1;
    g->size_z = (int)ceilf((max_z + margin - g->origin_z) / cell_size) + 1;

    /* Clamp to reasonable maximum to avoid huge allocations */
    if (g->size_x > 512) g->size_x = 512;
    if (g->size_y > 512) g->size_y = 512;
    if (g->size_z > 256) g->size_z = 256;

    size_t total = (size_t)g->size_x * g->size_y * g->size_z;
    g->cells = (bool*)calloc(total, sizeof(bool));
    if (!g->cells) {
        free(g);
        return NULL;
    }

    return g;
}

void occupancy_grid_destroy(OccupancyGrid* grid) {
    if (!grid) return;
    free(grid->cells);
    free(grid);
}

/* ---------- Fill ---------- */

void occupancy_grid_fill_box(OccupancyGrid* grid,
                              float x, float y, float z,
                              float sx, float sy, float sz) {
    /* Convert world-space box to grid cells */
    int x0 = world_to_cell(x, grid->origin_x, grid->cell_size);
    int y0 = world_to_cell(y, grid->origin_y, grid->cell_size);
    int z0 = world_to_cell(z, grid->origin_z, grid->cell_size);
    int x1 = world_to_cell(x + sx, grid->origin_x, grid->cell_size);
    int y1 = world_to_cell(y + sy, grid->origin_y, grid->cell_size);
    int z1 = world_to_cell(z + sz, grid->origin_z, grid->cell_size);

    /* Clamp to grid bounds */
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 >= grid->size_x) x1 = grid->size_x - 1;
    if (y1 >= grid->size_y) y1 = grid->size_y - 1;
    if (z1 >= grid->size_z) z1 = grid->size_z - 1;

    for (int gz = z0; gz <= z1; gz++) {
        for (int gy = y0; gy <= y1; gy++) {
            for (int gx = x0; gx <= x1; gx++) {
                grid->cells[grid_index(grid, gx, gy, gz)] = true;
            }
        }
    }
}

/* ---------- Query ---------- */

bool occupancy_grid_get(const OccupancyGrid* grid, int gx, int gy, int gz) {
    if (!in_bounds(grid, gx, gy, gz)) return false;
    return grid->cells[grid_index(grid, gx, gy, gz)];
}

bool occupancy_grid_sample(const OccupancyGrid* grid, float wx, float wy, float wz) {
    int gx = world_to_cell(wx, grid->origin_x, grid->cell_size);
    int gy = world_to_cell(wy, grid->origin_y, grid->cell_size);
    int gz = world_to_cell(wz, grid->origin_z, grid->cell_size);
    return occupancy_grid_get(grid, gx, gy, gz);
}

/* ---------- Vertex AO ---------- */

/*
 * Vertex AO uses the standard voxel AO algorithm:
 *
 * For a vertex at the corner of a face, sample 3 neighboring cells:
 *   - side1 (along one tangent axis)
 *   - side2 (along other tangent axis)
 *   - corner (diagonal)
 *
 * AO value:
 *   if (side1 && side2) → 0 (fully occluded corner)
 *   else → 3 - (side1 + side2 + corner)
 *
 * Normalized to [0, 1] by dividing by 3.
 *
 * This gives a smooth darkening effect at corners and seams.
 */
float occupancy_grid_vertex_ao(const OccupancyGrid* grid,
                                float wx, float wy, float wz,
                                int face_axis, int face_sign,
                                int corner_u, int corner_v) {
    /* Determine the two tangent axes based on face normal axis */
    int u_axis, v_axis;
    if (face_axis == 0) {        /* X face → tangents Y, Z */
        u_axis = 1; v_axis = 2;
    } else if (face_axis == 1) { /* Y face → tangents X, Z */
        u_axis = 0; v_axis = 2;
    } else {                     /* Z face → tangents X, Y */
        u_axis = 0; v_axis = 1;
    }

    /* Convert vertex world position to grid coordinates */
    int gx = world_to_cell(wx, grid->origin_x, grid->cell_size);
    int gy = world_to_cell(wy, grid->origin_y, grid->cell_size);
    int gz = world_to_cell(wz, grid->origin_z, grid->cell_size);

    /* The neighbor offset in the normal direction (outside the face) */
    int n_off[3] = {0, 0, 0};
    n_off[face_axis] = face_sign;

    /* side1: offset along u tangent */
    int s1[3] = {gx + n_off[0], gy + n_off[1], gz + n_off[2]};
    if (u_axis == 0) s1[0] += corner_u;
    else if (u_axis == 1) s1[1] += corner_u;
    else s1[2] += corner_u;

    /* side2: offset along v tangent */
    int s2[3] = {gx + n_off[0], gy + n_off[1], gz + n_off[2]};
    if (v_axis == 0) s2[0] += corner_v;
    else if (v_axis == 1) s2[1] += corner_v;
    else s2[2] += corner_v;

    /* corner: offset along both tangents */
    int cr[3] = {gx + n_off[0], gy + n_off[1], gz + n_off[2]};
    if (u_axis == 0) cr[0] += corner_u;
    else if (u_axis == 1) cr[1] += corner_u;
    else cr[2] += corner_u;
    if (v_axis == 0) cr[0] += corner_v;
    else if (v_axis == 1) cr[1] += corner_v;
    else cr[2] += corner_v;

    bool side1  = occupancy_grid_get(grid, s1[0], s1[1], s1[2]);
    bool side2  = occupancy_grid_get(grid, s2[0], s2[1], s2[2]);
    bool corner = occupancy_grid_get(grid, cr[0], cr[1], cr[2]);

    int ao;
    if (side1 && side2) {
        ao = 0;
    } else {
        ao = 3 - ((int)side1 + (int)side2 + (int)corner);
    }

    /* Map: 0→0.4, 1→0.6, 2→0.8, 3→1.0 for a subtler AO effect */
    static const float ao_table[4] = {0.4f, 0.6f, 0.8f, 1.0f};
    return ao_table[ao];
}
