#ifndef ACTOR_RENDER_H
#define ACTOR_RENDER_H

#include "render/voxel_mesh.h"

/*
 * Actor mesh generation and rendering.
 *
 * Each actor is composed of multiple VoxelMesh components:
 *   - body:  main cube, subdivided (4×4 quads per face) for jelly deformation
 *   - horns: Minotaur only — white nubs protruding from the top face (rigid)
 *   - face:  Minotaur only — detail voxels on camera-facing side (rigid)
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
    VoxelMesh face;         /* Minotaur face detail (rigid, no subdivision) */
    float     body_height;  /* Height of the body for u_deform_height */
    bool      has_horns;    /* true if horns mesh is built */
    bool      has_face;     /* true if face mesh is built */
} ActorParts;

/* Build Theseus actor parts.
 * Body: beveled blue cube, ~45% tile size, RGB(80, 168, 251).
 * Subdivision=4 for deformation support (~96 verts per face). */
void actor_render_build_theseus(ActorParts* parts);

/* Build Minotaur actor parts.
 * Body: red cube, ~65% tile size × 80% height, RGB(239, 34, 34).
 * Horns: white nubs on top face.
 * Face: brow, eyes, snout on -Z (camera-facing) side.
 * Body subdivision=4 for deformation; horns and face are rigid. */
void actor_render_build_minotaur(ActorParts* parts);

/* Release all GPU resources for an actor. Safe on zero-initialized struct. */
void actor_render_destroy(ActorParts* parts);

#endif /* ACTOR_RENDER_H */
