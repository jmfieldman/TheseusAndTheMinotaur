#include "render/actor_render.h"
#include "engine/utils.h"

#include <string.h>

/* ---------- Theseus geometry constants ---------- */

/* Theseus body: ~45% of one tile */
#define THESEUS_SIZE    0.45f
/* Subdivision level for deformable body mesh */
#define ACTOR_SUBDIVISIONS 4

/* ---------- Minotaur geometry constants ---------- */

/* Minotaur body: ~65% of one tile, 80% height ratio */
#define MINOTAUR_SIZE   0.65f
#define MINOTAUR_HEIGHT_RATIO 0.8f

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

/* Face detail dimensions (on -Z face, camera-facing with yaw=0) */
#define FACE_PROTRUDE  0.025f  /* how far details stick out from face */

/* Brow: wide dark bar above eyes */
#define BROW_WIDTH   0.22f
#define BROW_HEIGHT  0.035f
#define BROW_Y_FRAC  0.68f   /* fraction of body_h */

/* Eyes: small bright squares */
#define EYE_SIZE     0.055f
#define EYE_Y_FRAC   0.52f
#define EYE_X_OFFSET 0.08f   /* distance from center to eye center */

/* Snout: wider protruding box below eyes */
#define SNOUT_WIDTH  0.14f
#define SNOUT_HEIGHT 0.07f
#define SNOUT_DEPTH  0.04f
#define SNOUT_Y_FRAC 0.32f

/* ---------- Occluder ---------- */

/* Invisible ground plane for AO baking.
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
    voxel_mesh_set_subdivisions(&parts->body, ACTOR_SUBDIVISIONS);

    /* Main body cube */
    voxel_mesh_add_box_ex(&parts->body,
                           -half, 0.0f, -half,
                           size, size, size,
                           80.0f / 255.0f, 168.0f / 255.0f, 251.0f / 255.0f, 1.0f,
                           true, AO_MODE_ATLAS);

    /* Subtle top trim — slightly darker lip for visual definition */
    voxel_mesh_set_subdivisions(&parts->body, 1);
    {
        float trim_h = 0.018f;
        float trim_inset = 0.003f;
        voxel_mesh_add_box_ex(&parts->body,
                               -half + trim_inset, size - trim_h, -half + trim_inset,
                               size - 2.0f * trim_inset, trim_h, size - 2.0f * trim_inset,
                               68.0f / 255.0f, 143.0f / 255.0f, 213.0f / 255.0f, 1.0f,
                               true, AO_MODE_ATLAS);
    }

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
    voxel_mesh_set_subdivisions(&parts->body, ACTOR_SUBDIVISIONS);

    /* Main body cube */
    voxel_mesh_add_box_ex(&parts->body,
                           -half, 0.0f, -half,
                           size, body_h, size,
                           MINO_R, MINO_G, MINO_B, 1.0f,
                           true, AO_MODE_ATLAS);

    /* Subtle top trim */
    voxel_mesh_set_subdivisions(&parts->body, 1);
    {
        float trim_h = 0.020f;
        float trim_inset = 0.003f;
        voxel_mesh_add_box_ex(&parts->body,
                               -half + trim_inset, body_h - trim_h, -half + trim_inset,
                               size - 2.0f * trim_inset, trim_h, size - 2.0f * trim_inset,
                               MINO_R * 0.80f, MINO_G * 0.80f, MINO_B * 0.80f, 1.0f,
                               true, AO_MODE_ATLAS);
    }

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

    /* ── Face mesh (rigid, on -Z face — camera-facing with yaw=0) ── */
    parts->has_face = true;
    voxel_mesh_begin(&parts->face);

    float face_z = -half;  /* front face of the body */

    /* Brow: wide dark bar above eyes */
    {
        float brow_y = body_h * BROW_Y_FRAC;
        voxel_mesh_add_box(&parts->face,
                            -BROW_WIDTH * 0.5f, brow_y, face_z - FACE_PROTRUDE,
                            BROW_WIDTH, BROW_HEIGHT, FACE_PROTRUDE,
                            0.60f, 0.08f, 0.08f, 1.0f,
                            true);
    }

    /* Eyes: bright squares for contrast */
    {
        float eye_y = body_h * EYE_Y_FRAC;
        /* Left eye */
        voxel_mesh_add_box(&parts->face,
                            -EYE_X_OFFSET - EYE_SIZE * 0.5f, eye_y,
                            face_z - FACE_PROTRUDE,
                            EYE_SIZE, EYE_SIZE, FACE_PROTRUDE,
                            0.95f, 0.90f, 0.85f, 1.0f,
                            true);
        /* Right eye */
        voxel_mesh_add_box(&parts->face,
                            EYE_X_OFFSET - EYE_SIZE * 0.5f, eye_y,
                            face_z - FACE_PROTRUDE,
                            EYE_SIZE, EYE_SIZE, FACE_PROTRUDE,
                            0.95f, 0.90f, 0.85f, 1.0f,
                            true);
    }

    /* Snout: wider protruding box below eyes */
    {
        float snout_y = body_h * SNOUT_Y_FRAC;
        voxel_mesh_add_box(&parts->face,
                            -SNOUT_WIDTH * 0.5f, snout_y,
                            face_z - SNOUT_DEPTH,
                            SNOUT_WIDTH, SNOUT_HEIGHT, SNOUT_DEPTH,
                            0.80f, 0.18f, 0.12f, 1.0f,
                            true);
    }

    /* Occluder: the body, so face details get AO from the body surface */
    voxel_mesh_add_occluder(&parts->face,
                             -half, 0.0f, -half, size, body_h, size);
    voxel_mesh_build(&parts->face, 0.03f);

    LOG_DEBUG("Minotaur built: body %d verts, horns %d verts, face %d verts",
              voxel_mesh_get_vertex_count(&parts->body),
              voxel_mesh_get_vertex_count(&parts->horns),
              voxel_mesh_get_vertex_count(&parts->face));
}

void actor_render_destroy(ActorParts* parts) {
    voxel_mesh_destroy(&parts->body);
    if (parts->has_horns) voxel_mesh_destroy(&parts->horns);
    if (parts->has_face)  voxel_mesh_destroy(&parts->face);
    memset(parts, 0, sizeof(ActorParts));
}
