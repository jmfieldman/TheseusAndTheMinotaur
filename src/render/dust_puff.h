#ifndef DUST_PUFF_H
#define DUST_PUFF_H

#include <stdbool.h>

/*
 * Dust puff particle system.
 *
 * Spawns cel-shaded billboard circles around an impact point.
 * Each particle drifts outward/upward, scales up, rotates, and fades.
 * Rendered as camera-facing quads with a procedural SDF circle shader.
 *
 * Usage:
 *   dust_puff_init();               // once at startup
 *   dust_puff_spawn(x, y, z);       // on minotaur landing
 *   dust_puff_update(dt);           // each frame
 *   dust_puff_render(vp_matrix);    // after diorama, with depth test on
 *   dust_puff_shutdown();           // cleanup
 */

/* Initialize GL resources (shader, VAO/VBO). Call once. */
void dust_puff_init(void);

/* Release GL resources. */
void dust_puff_shutdown(void);

/* Spawn a burst of dust puff particles at world position (x, y, z). */
void dust_puff_spawn(float x, float y, float z);

/* Advance all live particles by dt seconds. */
void dust_puff_update(float dt);

/* Render all live particles.
 * vp: the 4x4 view-projection matrix (column-major).
 * view: the 4x4 view matrix (column-major) — used for billboarding.
 * Returns true if any particles were drawn. */
bool dust_puff_render(const float* vp, const float* view);

/* Returns true if any particles are currently alive. */
bool dust_puff_is_active(void);

#endif /* DUST_PUFF_H */
