#include "render/voxel_mesh.h"
#include "render/occupancy_grid.h"
#include "engine/utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ---------- Vertex layout ---------- */

/* 10 floats per vertex: pos(3) + normal(3) + color(4) */
#define FLOATS_PER_VERTEX 10
/* 6 vertices per face (2 triangles), 6 faces per box max = 36 verts */
#define MAX_VERTS_PER_BOX 36

/* ---------- Face emission helpers ---------- */

/*
 * Face vertex positions for a unit cube [0,1]^3.
 * Each face has 6 vertices (2 triangles), CCW winding when viewed from outside.
 *
 * Face order: +X, -X, +Y, -Y, +Z, -Z
 */

typedef struct {
    float nx, ny, nz;             /* face normal */
    int   face_axis;              /* 0=X, 1=Y, 2=Z */
    int   face_sign;              /* +1 or -1 */
    float verts[6][3];            /* 6 vertices, each (x,y,z) in [0,1] */
    int   corner_u[6];            /* per-vertex corner_u for AO (-1 or +1) */
    int   corner_v[6];            /* per-vertex corner_v for AO (-1 or +1) */
} FaceTemplate;

static const FaceTemplate s_faces[6] = {
    /* +X face (right) */
    { 1, 0, 0,  0, +1,
      {{1,0,0}, {1,1,0}, {1,1,1}, {1,0,0}, {1,1,1}, {1,0,1}},
      {-1, +1, +1, -1, +1, -1},   /* corner_u along Y */
      {-1, -1, +1, -1, +1, +1},   /* corner_v along Z */
    },
    /* -X face (left) */
    { -1, 0, 0,  0, -1,
      {{0,0,0}, {0,0,1}, {0,1,1}, {0,0,0}, {0,1,1}, {0,1,0}},
      {-1, -1, +1, -1, +1, +1},
      {-1, +1, +1, -1, +1, -1},
    },
    /* +Y face (top) */
    { 0, 1, 0,  1, +1,
      {{0,1,0}, {0,1,1}, {1,1,1}, {0,1,0}, {1,1,1}, {1,1,0}},
      {-1, -1, +1, -1, +1, +1},
      {-1, +1, +1, -1, +1, -1},
    },
    /* -Y face (bottom) */
    { 0, -1, 0,  1, -1,
      {{0,0,0}, {1,0,0}, {1,0,1}, {0,0,0}, {1,0,1}, {0,0,1}},
      {-1, +1, +1, -1, +1, -1},
      {-1, -1, +1, -1, +1, +1},
    },
    /* +Z face (front) */
    { 0, 0, 1,  2, +1,
      {{0,0,1}, {1,0,1}, {1,1,1}, {0,0,1}, {1,1,1}, {0,1,1}},
      {-1, +1, +1, -1, +1, -1},
      {-1, -1, +1, -1, +1, +1},
    },
    /* -Z face (back) */
    { 0, 0, -1,  2, -1,
      {{0,0,0}, {0,1,0}, {1,1,0}, {0,0,0}, {1,1,0}, {1,0,0}},
      {-1, +1, +1, -1, +1, -1},
      {+1, +1, -1, +1, -1, -1},
    },
};

/* Check if a face of a box is hidden by sampling the occupancy grid
 * at the center of the adjacent cell on the outside of the face. */
static bool face_is_hidden(const OccupancyGrid* occ,
                            const VoxelBox* box,
                            const FaceTemplate* face) {
    /* Sample point: center of the box face, then offset by half a cell
     * size in the normal direction to sample the neighboring cell. */
    float cx = box->x + box->sx * 0.5f;
    float cy = box->y + box->sy * 0.5f;
    float cz = box->z + box->sz * 0.5f;

    /* Move to face center */
    if (face->face_axis == 0) {
        cx = (face->face_sign > 0) ? box->x + box->sx : box->x;
    } else if (face->face_axis == 1) {
        cy = (face->face_sign > 0) ? box->y + box->sy : box->y;
    } else {
        cz = (face->face_sign > 0) ? box->z + box->sz : box->z;
    }

    /* Offset slightly into the neighbor */
    float eps = occ->cell_size * 0.5f;
    cx += face->nx * eps;
    cy += face->ny * eps;
    cz += face->nz * eps;

    return occupancy_grid_sample(occ, cx, cy, cz);
}

/* ---------- Public API ---------- */

void voxel_mesh_begin(VoxelMesh* mesh) {
    memset(mesh, 0, sizeof(VoxelMesh));
}

void voxel_mesh_add_box(VoxelMesh* mesh,
                         float x, float y, float z,
                         float sx, float sy, float sz,
                         float r, float g, float b, float a) {
    if (mesh->box_count >= VOXEL_MESH_MAX_BOXES) {
        LOG_WARN("VoxelMesh: max boxes (%d) exceeded, ignoring", VOXEL_MESH_MAX_BOXES);
        return;
    }
    VoxelBox* box = &mesh->boxes[mesh->box_count++];
    box->x = x; box->y = y; box->z = z;
    box->sx = sx; box->sy = sy; box->sz = sz;
    box->r = r; box->g = g; box->b = b; box->a = a;
}

void voxel_mesh_build(VoxelMesh* mesh, float cell_size) {
    if (mesh->box_count == 0) {
        mesh->built = true;
        mesh->vertex_count = 0;
        return;
    }

    /* ---------- 1. Compute AABB of all boxes ---------- */
    float min_x = FLT_MAX, min_y = FLT_MAX, min_z = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX, max_z = -FLT_MAX;

    for (int i = 0; i < mesh->box_count; i++) {
        const VoxelBox* b = &mesh->boxes[i];
        if (b->x < min_x) min_x = b->x;
        if (b->y < min_y) min_y = b->y;
        if (b->z < min_z) min_z = b->z;
        if (b->x + b->sx > max_x) max_x = b->x + b->sx;
        if (b->y + b->sy > max_y) max_y = b->y + b->sy;
        if (b->z + b->sz > max_z) max_z = b->z + b->sz;
    }

    /* ---------- 2. Create and populate occupancy grid ---------- */
    OccupancyGrid* occ = occupancy_grid_create(min_x, min_y, min_z,
                                                 max_x, max_y, max_z,
                                                 cell_size);
    if (!occ) {
        LOG_ERROR("VoxelMesh: failed to create occupancy grid");
        mesh->built = true;
        mesh->vertex_count = 0;
        return;
    }

    for (int i = 0; i < mesh->box_count; i++) {
        const VoxelBox* b = &mesh->boxes[i];
        occupancy_grid_fill_box(occ, b->x, b->y, b->z, b->sx, b->sy, b->sz);
    }

    /* ---------- 3. Emit vertices with culling and AO ---------- */
    /* Allocate worst-case vertex buffer */
    int max_vertices = mesh->box_count * MAX_VERTS_PER_BOX;
    float* verts = (float*)malloc((size_t)max_vertices * FLOATS_PER_VERTEX * sizeof(float));
    if (!verts) {
        LOG_ERROR("VoxelMesh: failed to allocate vertex buffer");
        occupancy_grid_destroy(occ);
        mesh->built = true;
        mesh->vertex_count = 0;
        return;
    }

    int vert_count = 0;

    for (int i = 0; i < mesh->box_count; i++) {
        const VoxelBox* box = &mesh->boxes[i];

        for (int f = 0; f < 6; f++) {
            const FaceTemplate* face = &s_faces[f];

            /* Skip hidden faces */
            if (face_is_hidden(occ, box, face)) {
                continue;
            }

            /* Emit 6 vertices for this face */
            for (int v = 0; v < 6; v++) {
                float* dst = &verts[vert_count * FLOATS_PER_VERTEX];

                /* Position: scale template [0,1] by box size + offset */
                float px = box->x + face->verts[v][0] * box->sx;
                float py = box->y + face->verts[v][1] * box->sy;
                float pz = box->z + face->verts[v][2] * box->sz;

                dst[0] = px;
                dst[1] = py;
                dst[2] = pz;

                /* Normal */
                dst[3] = face->nx;
                dst[4] = face->ny;
                dst[5] = face->nz;

                /* AO: sample at vertex position */
                float ao = occupancy_grid_vertex_ao(occ, px, py, pz,
                                                     face->face_axis,
                                                     face->face_sign,
                                                     face->corner_u[v],
                                                     face->corner_v[v]);

                /* Color with baked AO */
                dst[6] = box->r * ao;
                dst[7] = box->g * ao;
                dst[8] = box->b * ao;
                dst[9] = box->a;

                vert_count++;
            }
        }
    }

    /* ---------- 4. Upload to GPU ---------- */
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);

    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(vert_count * FLOATS_PER_VERTEX * sizeof(float)),
                 verts, GL_STATIC_DRAW);

    /* position: location 0, vec3 */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          FLOATS_PER_VERTEX * sizeof(float), (void*)0);

    /* normal: location 1, vec3 */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          FLOATS_PER_VERTEX * sizeof(float),
                          (void*)(3 * sizeof(float)));

    /* color: location 2, vec4 */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
                          FLOATS_PER_VERTEX * sizeof(float),
                          (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    mesh->vertex_count = vert_count;
    mesh->built = true;

    /* ---------- 5. Cleanup ---------- */
    free(verts);
    occupancy_grid_destroy(occ);
    mesh->box_count = 0;  /* Clear CPU staging data */

    LOG_DEBUG("VoxelMesh built: %d vertices (%d triangles)",
              vert_count, vert_count / 3);
}

void voxel_mesh_draw(const VoxelMesh* mesh) {
    if (!mesh->built || mesh->vertex_count == 0) return;

    glBindVertexArray(mesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertex_count);
    glBindVertexArray(0);
}

void voxel_mesh_destroy(VoxelMesh* mesh) {
    if (mesh->vao) glDeleteVertexArrays(1, &mesh->vao);
    if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
    memset(mesh, 0, sizeof(VoxelMesh));
}

int voxel_mesh_get_vertex_count(const VoxelMesh* mesh) {
    return mesh->vertex_count;
}
