#include "render/actor_render.h"
#include "render/shader.h"
#include "engine/utils.h"

#include <string.h>

/* ---------- Theseus geometry constants ---------- */

/* Theseus body: ~45% of one tile */
#define THESEUS_SIZE    0.45f
/* Subdivision level for deformable body mesh */
#define ACTOR_SUBDIVISIONS 4

/* ---------- Minotaur geometry constants ---------- */

/* Minotaur body: ~65% of one tile, true cube */
#define MINOTAUR_SIZE   0.65f
#define MINOTAUR_HEIGHT_RATIO 1.0f  /* true cube — rolls land on an identical face */

/* Minotaur colors */
#define MINO_R (239.0f / 255.0f)
#define MINO_G ( 34.0f / 255.0f)
#define MINO_B ( 34.0f / 255.0f)

/* Horn dimensions */
#define HORN_WIDTH    0.06f
#define HORN_HEIGHT   0.10f
#define HORN_DEPTH    0.06f
#define HORN_SPACING  0.14f   /* center-to-center distance between horns */
#define HORN_Z_OFFSET (-0.04f) /* slightly forward of center */

/* ---------- Occluder ---------- */

/* Invisible ground plane for AO baking (used by horn/face meshes).
 * Large enough to simulate an infinite floor under the actor. */
static void add_ground_occluder(VoxelMesh* mesh) {
    voxel_mesh_add_occluder(mesh, -2.0f, -0.05f, -2.0f, 4.0f, 0.05f, 4.0f);
}

/* ---------- Public API ---------- */

void actor_render_build_theseus(ActorParts* parts) {
    memset(parts, 0, sizeof(ActorParts));

    float size = THESEUS_SIZE;
    float half = size * 0.5f;
    parts->body_height = size;

    /* Body mesh (subdivided for deformation) */
    voxel_mesh_begin(&parts->body);
    parts->body.analytical_ao = true;  /* smooth gradients, no raytracing */
    voxel_mesh_set_subdivisions(&parts->body, ACTOR_SUBDIVISIONS);

    /* Main body cube — analytical AO produces smooth ground-proximity
     * gradients without the banding artifacts from coarse occupancy grids. */
    voxel_mesh_add_box_ex(&parts->body,
                           -half, 0.0f, -half,
                           size, size, size,
                           80.0f / 255.0f, 168.0f / 255.0f, 251.0f / 255.0f, 1.0f,
                           true, AO_MODE_ATLAS);

    add_ground_occluder(&parts->body);
    voxel_mesh_build(&parts->body, size * 0.25f);

    LOG_DEBUG("Theseus built: body %d verts",
              voxel_mesh_get_vertex_count(&parts->body));
}

void actor_render_build_minotaur(ActorParts* parts) {
    memset(parts, 0, sizeof(ActorParts));

    float size = MINOTAUR_SIZE;
    float half = size * 0.5f;
    float body_h = size * MINOTAUR_HEIGHT_RATIO;
    parts->body_height = body_h;

    /* ── Body mesh (subdivided for deformation) ────────── */
    voxel_mesh_begin(&parts->body);
    parts->body.analytical_ao = true;  /* smooth gradients, no raytracing */
    voxel_mesh_set_subdivisions(&parts->body, ACTOR_SUBDIVISIONS);

    /* Main body cube — analytical AO for smooth ground-proximity gradients */
    voxel_mesh_add_box_ex(&parts->body,
                           -half, 0.0f, -half,
                           size, body_h, size,
                           MINO_R, MINO_G, MINO_B, 1.0f,
                           true, AO_MODE_ATLAS);

    add_ground_occluder(&parts->body);
    voxel_mesh_build(&parts->body, size * 0.25f);

    /* ── Horn mesh (rigid) ─────────────────────────────── */
    parts->has_horns = true;
    voxel_mesh_begin(&parts->horns);

    /* Left horn */
    voxel_mesh_add_box(&parts->horns,
                        -HORN_SPACING * 0.5f - HORN_WIDTH, body_h, HORN_Z_OFFSET,
                        HORN_WIDTH, HORN_HEIGHT, HORN_DEPTH,
                        0.95f, 0.95f, 0.90f, 1.0f,
                        true);
    /* Right horn */
    voxel_mesh_add_box(&parts->horns,
                        HORN_SPACING * 0.5f, body_h, HORN_Z_OFFSET,
                        HORN_WIDTH, HORN_HEIGHT, HORN_DEPTH,
                        0.95f, 0.95f, 0.90f, 1.0f,
                        true);

    /* Occluder: the body below the horns, for AO darkening at horn base */
    voxel_mesh_add_occluder(&parts->horns,
                             -half, 0.0f, -half, size, body_h, size);
    voxel_mesh_build(&parts->horns, HORN_WIDTH * 0.5f);

    LOG_DEBUG("Minotaur built: body %d verts, horns %d verts",
              voxel_mesh_get_vertex_count(&parts->body),
              voxel_mesh_get_vertex_count(&parts->horns));
}

void actor_render_destroy(ActorParts* parts) {
    voxel_mesh_destroy(&parts->body);
    if (parts->has_horns) voxel_mesh_destroy(&parts->horns);
    memset(parts, 0, sizeof(ActorParts));
}

/* ---------- Deformation state ---------- */

void deform_state_apply(const DeformState* ds, GLuint shader) {
    shader_set_float(shader, "u_deform_squash", ds->squash);
    shader_set_float(shader, "u_deform_flare",  ds->flare);
    shader_set_vec2(shader,  "u_deform_lean",   ds->lean_x, ds->lean_z);
    shader_set_vec2(shader,  "u_deform_squish_dir", ds->squish_dir_x, ds->squish_dir_z);
    shader_set_float(shader, "u_deform_squish", ds->squish_amount);
}
