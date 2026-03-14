#include "render/voxel_mesh.h"
#include "render/occupancy_grid.h"
#include "render/ao_baker.h"
#include "engine/utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ---------- Vertex layout ---------- */

/* 12 floats per vertex: pos(3) + normal(3) + color(4) + uv(2) */
#define FLOATS_PER_VERTEX 12
/* 6 vertices per face (2 triangles), 6 faces per box max = 36 verts */
#define MAX_VERTS_PER_BOX 36

/* ---------- Face emission helpers ---------- */

/*
 * Face vertex positions for a unit cube [0,1]^3.
 * Each face has 6 vertices (2 triangles), CCW winding when viewed from outside.
 *
 * Face order: +X, -X, +Y, -Y, +Z, -Z
 *
 * uv_coords: per-vertex UV in [0,1] for the face's local 2D space.
 * These map the face rectangle to a texture tile.
 */

typedef struct {
    float nx, ny, nz;             /* face normal */
    int   face_axis;              /* 0=X, 1=Y, 2=Z */
    int   face_sign;              /* +1 or -1 */
    float verts[6][3];            /* 6 vertices, each (x,y,z) in [0,1] */
    float uvs[6][2];             /* per-vertex UV in face-local [0,1] space */
} FaceTemplate;

/*
 * UV mapping per face:
 * For each face, the UV coordinates map the two tangent axes of the face
 * to U and V in [0,1].
 *
 * Face tangent axes:
 *   +X, -X: U=Z, V=Y  (but we adjust for winding)
 *   +Y, -Y: U=X, V=Z
 *   +Z, -Z: U=X, V=Y
 *
 * The UV values are derived from the vertex template positions
 * projected onto the face's tangent axes.
 */

static const FaceTemplate s_faces[6] = {
    /* +X face (right): tangent U=Z, V=Y */
    { 1, 0, 0,  0, +1,
      {{1,0,0}, {1,1,0}, {1,1,1}, {1,0,0}, {1,1,1}, {1,0,1}},
      {{0,0}, {0,1}, {1,1}, {0,0}, {1,1}, {1,0}},
    },
    /* -X face (left): tangent U=Z, V=Y */
    { -1, 0, 0,  0, -1,
      {{0,0,0}, {0,0,1}, {0,1,1}, {0,0,0}, {0,1,1}, {0,1,0}},
      {{0,0}, {1,0}, {1,1}, {0,0}, {1,1}, {0,1}},
    },
    /* +Y face (top): tangent U=X, V=Z */
    { 0, 1, 0,  1, +1,
      {{0,1,0}, {0,1,1}, {1,1,1}, {0,1,0}, {1,1,1}, {1,1,0}},
      {{0,0}, {0,1}, {1,1}, {0,0}, {1,1}, {1,0}},
    },
    /* -Y face (bottom): tangent U=X, V=Z */
    { 0, -1, 0,  1, -1,
      {{0,0,0}, {1,0,0}, {1,0,1}, {0,0,0}, {1,0,1}, {0,0,1}},
      {{0,0}, {1,0}, {1,1}, {0,0}, {1,1}, {0,1}},
    },
    /* +Z face (front): tangent U=X, V=Y */
    { 0, 0, 1,  2, +1,
      {{0,0,1}, {1,0,1}, {1,1,1}, {0,0,1}, {1,1,1}, {0,1,1}},
      {{0,0}, {1,0}, {1,1}, {0,0}, {1,1}, {0,1}},
    },
    /* -Z face (back): tangent U=X, V=Y */
    { 0, 0, -1,  2, -1,
      {{0,0,0}, {0,1,0}, {1,1,0}, {0,0,0}, {1,1,0}, {1,0,0}},
      {{0,0}, {0,1}, {1,1}, {0,0}, {1,1}, {1,0}},
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

    /* Offset into the neighbor by more than one cell to avoid sampling
     * a cell that the box itself partially occupies at its boundary. */
    float eps = occ->cell_size * 1.5f;
    cx += face->nx * eps;
    cy += face->ny * eps;
    cz += face->nz * eps;

    return occupancy_grid_sample(occ, cx, cy, cz);
}

/* ---------- AO atlas helpers ---------- */

/*
 * Compute the face's world-space origin and tangent axes for AO baking.
 *
 * face_origin: min corner of the face rectangle in world space
 * face_u_axis: vector from origin to max-U edge (length = face width)
 * face_v_axis: vector from origin to max-V edge (length = face height)
 */
static void compute_face_axes(const VoxelBox* box,
                               const FaceTemplate* face,
                               float face_origin[3],
                               float face_u_axis[3],
                               float face_v_axis[3]) {
    /* Find the min-UV vertex (UV = 0,0) and the two axis endpoints
     * from the face template. The first vertex in each face has UV (0,0). */

    /* Origin = box position + template vertex[0] * box size */
    face_origin[0] = box->x + face->verts[0][0] * box->sx;
    face_origin[1] = box->y + face->verts[0][1] * box->sy;
    face_origin[2] = box->z + face->verts[0][2] * box->sz;

    /* Find a vertex with UV (1,0) and one with UV (0,1) to determine axes.
     * Scan all 6 vertices for these. */
    float u_end[3] = {0}, v_end[3] = {0};
    bool found_u = false, found_v = false;

    for (int v = 0; v < 6; v++) {
        if (!found_u && face->uvs[v][0] > 0.5f && face->uvs[v][1] < 0.5f) {
            u_end[0] = box->x + face->verts[v][0] * box->sx;
            u_end[1] = box->y + face->verts[v][1] * box->sy;
            u_end[2] = box->z + face->verts[v][2] * box->sz;
            found_u = true;
        }
        if (!found_v && face->uvs[v][0] < 0.5f && face->uvs[v][1] > 0.5f) {
            v_end[0] = box->x + face->verts[v][0] * box->sx;
            v_end[1] = box->y + face->verts[v][1] * box->sy;
            v_end[2] = box->z + face->verts[v][2] * box->sz;
            found_v = true;
        }
    }

    /* If we didn't find exact (1,0) or (0,1) vertices, use the (1,1) vertex */
    if (!found_u || !found_v) {
        for (int v = 0; v < 6; v++) {
            if (face->uvs[v][0] > 0.5f && face->uvs[v][1] > 0.5f) {
                float corner[3] = {
                    box->x + face->verts[v][0] * box->sx,
                    box->y + face->verts[v][1] * box->sy,
                    box->z + face->verts[v][2] * box->sz,
                };
                if (!found_u) {
                    /* U axis: project onto face plane */
                    u_end[0] = corner[0];
                    u_end[1] = corner[1];
                    u_end[2] = corner[2];
                    /* Zero out V component by using origin's V position */
                    found_u = true;
                }
                if (!found_v) {
                    v_end[0] = corner[0];
                    v_end[1] = corner[1];
                    v_end[2] = corner[2];
                    found_v = true;
                }
                break;
            }
        }
    }

    face_u_axis[0] = u_end[0] - face_origin[0];
    face_u_axis[1] = u_end[1] - face_origin[1];
    face_u_axis[2] = u_end[2] - face_origin[2];

    face_v_axis[0] = v_end[0] - face_origin[0];
    face_v_axis[1] = v_end[1] - face_origin[1];
    face_v_axis[2] = v_end[2] - face_origin[2];
}

/* ---------- Public API ---------- */

void voxel_mesh_begin(VoxelMesh* mesh) {
    memset(mesh, 0, sizeof(VoxelMesh));
}

void voxel_mesh_add_box(VoxelMesh* mesh,
                         float x, float y, float z,
                         float sx, float sy, float sz,
                         float r, float g, float b, float a,
                         bool no_cull) {
    if (mesh->box_count >= VOXEL_MESH_MAX_BOXES) {
        LOG_WARN("VoxelMesh: max boxes (%d) exceeded, ignoring", VOXEL_MESH_MAX_BOXES);
        return;
    }
    VoxelBox* box = &mesh->boxes[mesh->box_count++];
    box->x = x; box->y = y; box->z = z;
    box->sx = sx; box->sy = sy; box->sz = sz;
    box->r = r; box->g = g; box->b = b; box->a = a;
    box->no_cull = no_cull;
    box->occluder_only = false;
}

void voxel_mesh_add_occluder(VoxelMesh* mesh,
                              float x, float y, float z,
                              float sx, float sy, float sz) {
    if (mesh->box_count >= VOXEL_MESH_MAX_BOXES) {
        LOG_WARN("VoxelMesh: max boxes (%d) exceeded, ignoring", VOXEL_MESH_MAX_BOXES);
        return;
    }
    VoxelBox* box = &mesh->boxes[mesh->box_count++];
    memset(box, 0, sizeof(VoxelBox));
    box->x = x; box->y = y; box->z = z;
    box->sx = sx; box->sy = sy; box->sz = sz;
    box->occluder_only = true;
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

    /* ---------- 3. First pass: count visible faces ---------- */
    int total_faces = 0;
    for (int i = 0; i < mesh->box_count; i++) {
        const VoxelBox* box = &mesh->boxes[i];
        if (box->occluder_only) continue;
        for (int f = 0; f < 6; f++) {
            if (!box->no_cull && face_is_hidden(occ, box, &s_faces[f])) {
                continue;
            }
            total_faces++;
        }
    }

    if (total_faces == 0) {
        occupancy_grid_destroy(occ);
        mesh->built = true;
        mesh->vertex_count = 0;
        return;
    }

    /* ---------- 4. Set up AO texture atlas ---------- */
    /* Pack tiles in row-major order. Each tile is AO_TILE_SIZE × AO_TILE_SIZE.
     * Add 0 padding between tiles (we'll use CLAMP_TO_EDGE on tile UVs). */
    int tile_px = AO_TILE_SIZE;  /* texels per tile side */
    int tiles_per_row = (int)ceilf(sqrtf((float)total_faces));
    if (tiles_per_row < 1) tiles_per_row = 1;
    int atlas_rows = (total_faces + tiles_per_row - 1) / tiles_per_row;

    int tex_w = tiles_per_row * tile_px;
    int tex_h = atlas_rows * tile_px;

    /* Round up to power of 2 for GPU friendliness */
    int pot_w = 1; while (pot_w < tex_w) pot_w <<= 1;
    int pot_h = 1; while (pot_h < tex_h) pot_h <<= 1;
    tex_w = pot_w;
    tex_h = pot_h;

    uint8_t* ao_data = (uint8_t*)calloc((size_t)tex_w * tex_h, sizeof(uint8_t));
    /* Default to fully lit (255) */
    memset(ao_data, 255, (size_t)tex_w * tex_h);

    /* ---------- 5. Emit vertices with UVs and bake AO ---------- */
    int max_vertices = total_faces * 6;
    float* verts = (float*)malloc((size_t)max_vertices * FLOATS_PER_VERTEX * sizeof(float));
    if (!verts) {
        LOG_ERROR("VoxelMesh: failed to allocate vertex buffer");
        free(ao_data);
        occupancy_grid_destroy(occ);
        mesh->built = true;
        mesh->vertex_count = 0;
        return;
    }

    int vert_count = 0;
    int face_index = 0;
    uint8_t tile_texels[AO_TILE_SIZE * AO_TILE_SIZE];

    for (int i = 0; i < mesh->box_count; i++) {
        const VoxelBox* box = &mesh->boxes[i];
        if (box->occluder_only) continue;

        for (int f = 0; f < 6; f++) {
            const FaceTemplate* face = &s_faces[f];

            /* Skip hidden faces (unless no_cull) */
            if (!box->no_cull && face_is_hidden(occ, box, face)) {
                continue;
            }

            /* Compute atlas tile position */
            int tile_col = face_index % tiles_per_row;
            int tile_row = face_index / tiles_per_row;
            float tile_u0 = (float)(tile_col * tile_px) / (float)tex_w;
            float tile_v0 = (float)(tile_row * tile_px) / (float)tex_h;
            float tile_du = (float)tile_px / (float)tex_w;
            float tile_dv = (float)tile_px / (float)tex_h;

            /* Inset UVs by half a texel to avoid sampling neighboring tiles */
            float half_texel_u = 0.5f / (float)tex_w;
            float half_texel_v = 0.5f / (float)tex_h;

            /* Bake AO for this face */
            float face_origin[3], face_u_axis[3], face_v_axis[3];
            float face_normal[3] = { face->nx, face->ny, face->nz };
            compute_face_axes(box, face, face_origin, face_u_axis, face_v_axis);

            ao_baker_bake_face(tile_texels, face_origin, face_u_axis, face_v_axis,
                               face_normal, tile_px, occ);

            /* Apply surface effects: soft edge darkening + weathered grain.
             * Seed from box index + face index for deterministic variation. */
            uint32_t fx_seed = (uint32_t)(i * 6 + f) * 2654435761u;
            ao_baker_apply_surface_effects(tile_texels, tile_px, fx_seed,
                                           0.15f,   /* edge_width: ~1 texel of fade */
                                           0.12f,   /* edge_darkness: 12% darker at edges */
                                           0.06f);  /* grain: 6% subtle variation */

            /* Copy tile texels into atlas */
            int atlas_x = tile_col * tile_px;
            int atlas_y = tile_row * tile_px;
            for (int ty = 0; ty < tile_px; ty++) {
                for (int tx = 0; tx < tile_px; tx++) {
                    ao_data[(atlas_y + ty) * tex_w + (atlas_x + tx)] =
                        tile_texels[ty * tile_px + tx];
                }
            }

            /* Emit 6 vertices for this face */
            for (int v = 0; v < 6; v++) {
                float* dst = &verts[vert_count * FLOATS_PER_VERTEX];

                /* Position: scale template [0,1] by box size + offset */
                dst[0] = box->x + face->verts[v][0] * box->sx;
                dst[1] = box->y + face->verts[v][1] * box->sy;
                dst[2] = box->z + face->verts[v][2] * box->sz;

                /* Normal */
                dst[3] = face->nx;
                dst[4] = face->ny;
                dst[5] = face->nz;

                /* Color (no AO baked in — AO comes from texture now) */
                dst[6] = box->r;
                dst[7] = box->g;
                dst[8] = box->b;
                dst[9] = box->a;

                /* UV: map face-local [0,1] UV to atlas tile region.
                 * Inset by half a texel to sample texel centers. */
                float u = face->uvs[v][0];
                float vv = face->uvs[v][1];
                dst[10] = tile_u0 + half_texel_u + u * (tile_du - 2.0f * half_texel_u);
                dst[11] = tile_v0 + half_texel_v + vv * (tile_dv - 2.0f * half_texel_v);

                vert_count++;
            }

            face_index++;
        }
    }

    /* ---------- 6. Upload geometry to GPU ---------- */
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

    /* uv: location 3, vec2 */
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE,
                          FLOATS_PER_VERTEX * sizeof(float),
                          (void*)(10 * sizeof(float)));

    glBindVertexArray(0);

    /* ---------- 7. Upload AO texture ---------- */
    glGenTextures(1, &mesh->ao_texture);
    glBindTexture(GL_TEXTURE_2D, mesh->ao_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, tex_w, tex_h, 0,
                 GL_RED, GL_UNSIGNED_BYTE, ao_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    mesh->ao_tex_width = tex_w;
    mesh->ao_tex_height = tex_h;
    mesh->has_ao = true;

    mesh->vertex_count = vert_count;
    mesh->built = true;

    /* ---------- 8. Cleanup ---------- */
    free(verts);
    free(ao_data);
    occupancy_grid_destroy(occ);
    mesh->box_count = 0;  /* Clear CPU staging data */

    LOG_DEBUG("VoxelMesh built: %d vertices (%d triangles), AO atlas %dx%d (%d faces)",
              vert_count, vert_count / 3, tex_w, tex_h, face_index);
}

void voxel_mesh_draw(const VoxelMesh* mesh) {
    if (!mesh->built || mesh->vertex_count == 0) return;

    /* Bind AO texture to unit 0 if available */
    if (mesh->has_ao && mesh->ao_texture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh->ao_texture);
    }

    glBindVertexArray(mesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertex_count);
    glBindVertexArray(0);

    if (mesh->has_ao && mesh->ao_texture) {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void voxel_mesh_destroy(VoxelMesh* mesh) {
    if (mesh->vao) glDeleteVertexArrays(1, &mesh->vao);
    if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
    if (mesh->ao_texture) glDeleteTextures(1, &mesh->ao_texture);
    memset(mesh, 0, sizeof(VoxelMesh));
}

int voxel_mesh_get_vertex_count(const VoxelMesh* mesh) {
    return mesh->vertex_count;
}

bool voxel_mesh_has_ao(const VoxelMesh* mesh) {
    return mesh->has_ao;
}
