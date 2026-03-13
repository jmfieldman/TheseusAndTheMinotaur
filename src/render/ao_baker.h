#ifndef AO_BAKER_H
#define AO_BAKER_H

#include "render/occupancy_grid.h"
#include <stdint.h>

/*
 * Raytraced ambient occlusion baker.
 *
 * For each texel on a face, casts hemisphere rays against an occupancy grid
 * to compute an occlusion value. The result is a grayscale AO map where
 * 255 = fully lit, 0 = fully occluded.
 *
 * This produces much higher quality AO than per-vertex sampling because
 * each texel gets its own independent occlusion computation, avoiding
 * interpolation artifacts across triangle boundaries.
 */

/* Number of hemisphere rays per texel. More = smoother but slower.
 * 32 is a good balance for small tile sizes (8×8 texels). */
#define AO_RAY_COUNT 32

/* Maximum ray march distance in grid cells. Controls how far AO influence
 * extends. 6 cells ≈ ~0.375 world units at cell_size=0.0625. */
#define AO_MAX_STEPS 6

/*
 * Bake AO for a single face tile.
 *
 * Writes tile_size × tile_size uint8_t values into `out_texels`.
 *
 * face_origin:  world-space position of the face's min corner
 * face_u_axis:  world-space direction of U axis across the face (length = face width)
 * face_v_axis:  world-space direction of V axis across the face (length = face height)
 * face_normal:  outward-facing normal of the face (unit length)
 * tile_size:    number of texels per side (e.g., 8)
 * grid:         occupancy grid for ray intersection testing
 */
void ao_baker_bake_face(uint8_t* out_texels,
                         const float face_origin[3],
                         const float face_u_axis[3],
                         const float face_v_axis[3],
                         const float face_normal[3],
                         int tile_size,
                         const OccupancyGrid* grid);

#endif /* AO_BAKER_H */
