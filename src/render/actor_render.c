#include "render/actor_render.h"
#include "render/shader.h"
#include "engine/utils.h"

#include <string.h>
#include <math.h>

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

/* Horn pyramid dimensions — true pyramids (4 triangular faces + square base).
 * Each horn base is ~25% of the body edge length, abutting the sides. */
#define HORN_BASE_W     0.16f   /* base width (X) — ~25% of 0.65 */
#define HORN_BASE_D     0.14f   /* base depth (Z) */
#define HORN_TOTAL_H    0.25f   /* total horn height (apex above base) */
#define HORN_X_OFFSET   0.24f   /* center-of-horn distance from body center (X) — near edge */
#define HORN_Z_OFFSET   (-0.08f) /* horn center offset toward front (-Z) */

/* Vertex layout must match voxel_mesh.c: 13 floats per vertex */
#define FLOATS_PER_VERTEX 13

/* ---------- Occluder ---------- */

/* Invisible ground plane for AO baking.
 * Large enough to simulate an infinite floor under the actor. */
static void add_ground_occluder(VoxelMesh* mesh) {
    voxel_mesh_add_occluder(mesh, -2.0f, -0.05f, -2.0f, 4.0f, 0.05f, 4.0f);
}

/* ---------- Pyramid helper ---------- */

/* Compute the face normal for a triangle (v0, v1, v2) via cross product.
 * Result is normalized in-place. */
static void tri_normal(const float v0[3], const float v1[3], const float v2[3],
                        float out[3]) {
    float e1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
    float e2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };
    out[0] = e1[1] * e2[2] - e1[2] * e2[1];
    out[1] = e1[2] * e2[0] - e1[0] * e2[2];
    out[2] = e1[0] * e2[1] - e1[1] * e2[0];
    float len = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (len > 1e-8f) { out[0] /= len; out[1] /= len; out[2] /= len; }
}

/* Emit one vertex (13 floats) into the buffer at the given offset. */
static void emit_vert(float* buf, int* idx,
                       float px, float py, float pz,
                       float nx, float ny, float nz,
                       float r, float g, float b, float a) {
    float* dst = &buf[(*idx) * FLOATS_PER_VERTEX];
    dst[0]  = px;  dst[1]  = py;  dst[2]  = pz;   /* position */
    dst[3]  = nx;  dst[4]  = ny;  dst[5]  = nz;   /* normal */
    dst[6]  = r;   dst[7]  = g;   dst[8]  = b;  dst[9] = a;  /* color */
    dst[10] = 0.0f; dst[11] = 0.0f;  /* uv (unused) */
    dst[12] = 1.0f;  /* ao_mode = ATLAS (>0.5 avoids wall heuristic stone pattern) */
    (*idx)++;
}

/*
 * Build a single pyramid: square base at y=base_y, apex at (cx, base_y+h, cz).
 * Base corners are at (cx ± hw, base_y, cz ± hd).
 * Emits 4 triangular side faces (12 verts) + 1 quad base face (6 verts) = 18 verts.
 */
static void build_pyramid(float* buf, int* idx,
                            float cx, float cz, float base_y,
                            float hw, float hd, float h,
                            float r, float g, float b, float a) {
    /* Base corners (Y = base_y):
     *   c0 = (-hw, 0, -hd)   c1 = (+hw, 0, -hd)
     *   c2 = (+hw, 0, +hd)   c3 = (-hw, 0, +hd)
     * Apex = (0, h, 0)  — all relative to (cx, base_y, cz) */
    float c[4][3] = {
        { cx - hw, base_y, cz - hd },  /* 0: front-left  (-X, -Z) */
        { cx + hw, base_y, cz - hd },  /* 1: front-right (+X, -Z) */
        { cx + hw, base_y, cz + hd },  /* 2: back-right  (+X, +Z) */
        { cx - hw, base_y, cz + hd },  /* 3: back-left   (-X, +Z) */
    };
    float apex[3] = { cx, base_y + h, cz };

    /* 4 triangular side faces: (c[i], c[(i+1)%4], apex) */
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        float n[3];
        tri_normal(c[i], c[j], apex, n);
        emit_vert(buf, idx, c[i][0], c[i][1], c[i][2], n[0], n[1], n[2], r, g, b, a);
        emit_vert(buf, idx, c[j][0], c[j][1], c[j][2], n[0], n[1], n[2], r, g, b, a);
        emit_vert(buf, idx, apex[0], apex[1], apex[2], n[0], n[1], n[2], r, g, b, a);
    }

    /* Base quad (facing down, normal = -Y) — two triangles */
    float bn[3] = { 0.0f, -1.0f, 0.0f };
    emit_vert(buf, idx, c[0][0], c[0][1], c[0][2], bn[0], bn[1], bn[2], r, g, b, a);
    emit_vert(buf, idx, c[2][0], c[2][1], c[2][2], bn[0], bn[1], bn[2], r, g, b, a);
    emit_vert(buf, idx, c[1][0], c[1][1], c[1][2], bn[0], bn[1], bn[2], r, g, b, a);
    emit_vert(buf, idx, c[0][0], c[0][1], c[0][2], bn[0], bn[1], bn[2], r, g, b, a);
    emit_vert(buf, idx, c[3][0], c[3][1], c[3][2], bn[0], bn[1], bn[2], r, g, b, a);
    emit_vert(buf, idx, c[2][0], c[2][1], c[2][2], bn[0], bn[1], bn[2], r, g, b, a);
}

/* Build a VoxelMesh from raw pyramid vertex data (bypasses the box system). */
static void build_horn_mesh(VoxelMesh* mesh, const float* verts, int vert_count) {
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);

    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(vert_count * FLOATS_PER_VERTEX * (int)sizeof(float)),
                 verts, GL_STATIC_DRAW);

    GLsizei stride = FLOATS_PER_VERTEX * (GLsizei)sizeof(float);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mesh->vertex_count = vert_count;
    mesh->built = true;
    mesh->has_ao = false;
    mesh->ao_texture = 0;
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

    /* ── Horn mesh (true pyramids, no AO) ────────────── */
    parts->has_horns = true;
    memset(&parts->horns, 0, sizeof(VoxelMesh));

    /* 2 pyramids × 18 verts each = 36 verts total */
    #define HORN_VERTS_PER_PYRAMID 18
    #define HORN_TOTAL_VERTS (2 * HORN_VERTS_PER_PYRAMID)
    float horn_buf[HORN_TOTAL_VERTS * FLOATS_PER_VERTEX];
    int vi = 0;

    float hw = HORN_BASE_W * 0.5f;
    float hd = HORN_BASE_D * 0.5f;

    /* Left horn (side = -1) — pure white */
    build_pyramid(horn_buf, &vi,
                  -HORN_X_OFFSET, HORN_Z_OFFSET, body_h,
                  hw, hd, HORN_TOTAL_H,
                  1.0f, 1.0f, 1.0f, 1.0f);

    /* Right horn (side = +1) — pure white */
    build_pyramid(horn_buf, &vi,
                  HORN_X_OFFSET, HORN_Z_OFFSET, body_h,
                  hw, hd, HORN_TOTAL_H,
                  1.0f, 1.0f, 1.0f, 1.0f);

    build_horn_mesh(&parts->horns, horn_buf, vi);

    LOG_DEBUG("Minotaur built: body %d verts, horns %d verts",
              voxel_mesh_get_vertex_count(&parts->body),
              parts->horns.vertex_count);
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
