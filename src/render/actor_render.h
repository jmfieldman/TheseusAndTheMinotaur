#ifndef ACTOR_RENDER_H
#define ACTOR_RENDER_H

#include "render/voxel_mesh.h"
#include <glad/gl.h>

/*
 * Actor mesh generation and rendering.
 *
 * Each actor is composed of multiple VoxelMesh components:
 *   - body:  main cube, subdivided (4×4 quads per face) for jelly deformation
 *   - horns: Minotaur only — white nubs protruding from the top face (rigid)
 *
 * All meshes are centered at the origin in XZ, with Y=0 at the base.
 * The caller positions actors via model-matrix translation.
 *
 * Body meshes include an invisible ground-plane occluder so the AO baker
 * produces darkened bottom edges.
 */

typedef struct {
    VoxelMesh body;         /* Main body mesh (subdivided for deformation) */
    VoxelMesh horns;        /* Minotaur horns (rigid, no subdivision) */
    float     body_height;  /* Height of the body for u_deform_height */
    bool      has_horns;    /* true if horns mesh is built */
} ActorParts;

/* Build Theseus actor parts.
 * Body: beveled blue cube, ~45% tile size, RGB(80, 168, 251).
 * Subdivision=4 for deformation support (~96 verts per face). */
void actor_render_build_theseus(ActorParts* parts);

/* Build Minotaur actor parts.
 * Body: red cube, ~65% tile size, RGB(239, 34, 34).
 * Horns: white nubs on top face.
 * Body subdivision=4 for deformation; horns are rigid. */
void actor_render_build_minotaur(ActorParts* parts);

/* Release all GPU resources for an actor. Safe on zero-initialized struct. */
void actor_render_destroy(ActorParts* parts);

/* ---------- Deformation state ---------- */

/*
 * DeformState — captures all deformation uniform values for one draw call.
 *
 * Used to compute per-frame deformation from animation progress (hop, wobble,
 * lean, etc.) and then apply all uniforms in one shot.
 */
typedef struct {
    float squash;        /* Y-axis scale (1.0 = identity) */
    float flare;         /* bottom XZ expansion (0.0 = none) */
    float lean_x;        /* XZ shear in X direction */
    float lean_z;        /* XZ shear in Z direction */
    float squish_dir_x;  /* directional squish axis X */
    float squish_dir_z;  /* directional squish axis Z */
    float squish_amount; /* squish strength (0.0 = none) */
} DeformState;

/* Set all fields to rest pose (no deformation). */
static inline void deform_state_identity(DeformState* ds) {
    ds->squash        = 1.0f;
    ds->flare         = 0.0f;
    ds->lean_x        = 0.0f;
    ds->lean_z        = 0.0f;
    ds->squish_dir_x  = 0.0f;
    ds->squish_dir_z  = 0.0f;
    ds->squish_amount = 0.0f;
}

/* Upload all deformation uniforms to the given shader program. */
void deform_state_apply(const DeformState* ds, GLuint shader);

#endif /* ACTOR_RENDER_H */
