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
 * 24 rays with 32×32 tile size gives smooth wall shadow gradients. */
#define AO_RAY_COUNT 24

/* Maximum ray march distance in grid cells. Controls how far AO influence
 * extends. 12 cells ≈ ~0.75 world units at cell_size=0.0625 — gives
 * wide soft shadows from walls onto adjacent floor tiles. */
#define AO_MAX_STEPS 12

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

/*
 * Apply surface effects to an already-baked AO tile.
 * Adds edge darkening (soft bevel) and grain noise (weathered texture).
 *
 * texels:        AO tile data (tile_size × tile_size uint8_t values)
 * tile_size:     number of texels per side
 * seed:          per-face seed for deterministic grain noise
 * edge_width:    fraction of face where edge darkening applies (0..0.5)
 * edge_darkness: maximum darkening at edge (0..1, e.g. 0.3 = 30% darker)
 * grain_amount:  grain noise amplitude (0..1, e.g. 0.1 = 10% variation)
 */
void ao_baker_apply_surface_effects(uint8_t* texels, int tile_size,
                                     uint32_t seed,
                                     float edge_width,
                                     float edge_darkness,
                                     float grain_amount);

/*
 * Analytical AO for a cube face sitting on a ground plane.
 *
 * Produces smooth gradients without occupancy grid artifacts:
 *   - Side faces: darker at bottom (ground proximity), smooth falloff upward
 *   - Top face:   slight edge darkening
 *   - Bottom face: mostly dark (pressed against ground)
 *
 * face_normal:  outward-facing normal of the face (unit length)
 * face_v_axis:  V axis of the face (used to determine which axis is "up")
 * tile_size:    number of texels per side
 * ground_dark:  maximum darkness at ground edge (0..1, e.g. 0.35)
 * ground_range: fraction of face height affected by ground gradient (0..1)
 */
void ao_baker_bake_face_analytical(uint8_t* out_texels,
                                    const float face_normal[3],
                                    const float face_v_axis[3],
                                    int tile_size,
                                    float ground_dark,
                                    float ground_range);

#endif /* AO_BAKER_H */
