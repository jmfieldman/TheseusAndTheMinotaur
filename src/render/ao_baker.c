#include "render/ao_baker.h"

#include <math.h>
#include <string.h>

/* ---------- Precomputed hemisphere directions ---------- */

/*
 * Fibonacci hemisphere: evenly distributed directions on the unit hemisphere
 * with z >= 0. Cosine-weighted by biasing the z component, so rays near
 * the surface normal contribute more (physically correct diffuse AO).
 *
 * These are precomputed once in a static array and rotated per-face
 * to align with the face normal.
 */

typedef struct {
    float x, y, z;
} Vec3;

/* Golden ratio for Fibonacci sphere */
#define PHI 1.6180339887498949f

/* Precomputed cosine-weighted hemisphere directions (z-up) */
static Vec3 s_hemi_dirs[AO_RAY_COUNT];
static bool s_hemi_initialized = false;

static void init_hemisphere_dirs(void) {
    if (s_hemi_initialized) return;

    for (int i = 0; i < AO_RAY_COUNT; i++) {
        /* Cosine-weighted hemisphere sampling:
         * z = sqrt(1 - u) where u is uniform in [0,1)
         * This biases toward the pole (normal direction). */
        float u = (float)i / (float)AO_RAY_COUNT;
        float z = sqrtf(1.0f - u);
        float r = sqrtf(1.0f - z * z);
        float theta = 2.0f * (float)M_PI * (float)i / PHI;

        s_hemi_dirs[i].x = r * cosf(theta);
        s_hemi_dirs[i].y = r * sinf(theta);
        s_hemi_dirs[i].z = z;
    }

    s_hemi_initialized = true;
}

/* ---------- Ray marching ---------- */

/*
 * March a ray through the occupancy grid. Returns true if the ray hits
 * an occupied cell within max_steps.
 *
 * Uses simple fixed-step marching at cell_size intervals. This is not
 * a precise DDA, but for AO purposes the approximation is fine and
 * much simpler to implement.
 */
static bool ray_hits_occupancy(const OccupancyGrid* grid,
                                float ox, float oy, float oz,
                                float dx, float dy, float dz,
                                int max_steps) {
    float step = grid->cell_size;

    for (int s = 1; s <= max_steps; s++) {
        float t = step * (float)s;
        float px = ox + dx * t;
        float py = oy + dy * t;
        float pz = oz + dz * t;

        if (occupancy_grid_sample(grid, px, py, pz)) {
            return true;
        }
    }

    return false;
}

/* ---------- Rotate hemisphere to face normal ---------- */

/*
 * Build a tangent-space basis from a normal vector.
 * Returns two tangent vectors (t, b) such that (t, b, n) form
 * a right-handed orthonormal basis.
 */
static void build_tangent_basis(const float n[3], float t[3], float b[3]) {
    /* Choose a vector not parallel to n */
    float ref[3];
    if (fabsf(n[1]) < 0.9f) {
        ref[0] = 0.0f; ref[1] = 1.0f; ref[2] = 0.0f;
    } else {
        ref[0] = 1.0f; ref[1] = 0.0f; ref[2] = 0.0f;
    }

    /* t = normalize(cross(n, ref)) */
    t[0] = n[1] * ref[2] - n[2] * ref[1];
    t[1] = n[2] * ref[0] - n[0] * ref[2];
    t[2] = n[0] * ref[1] - n[1] * ref[0];
    float len = sqrtf(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
    if (len > 1e-6f) {
        t[0] /= len; t[1] /= len; t[2] /= len;
    }

    /* b = cross(n, t) */
    b[0] = n[1] * t[2] - n[2] * t[1];
    b[1] = n[2] * t[0] - n[0] * t[2];
    b[2] = n[0] * t[1] - n[1] * t[0];
}

/* ---------- Public API ---------- */

void ao_baker_bake_face(uint8_t* out_texels,
                         const float face_origin[3],
                         const float face_u_axis[3],
                         const float face_v_axis[3],
                         const float face_normal[3],
                         int tile_size,
                         const OccupancyGrid* grid) {
    init_hemisphere_dirs();

    /* Build tangent-space basis from face normal */
    float tan1[3], tan2[3];
    build_tangent_basis(face_normal, tan1, tan2);

    /* Small offset along normal to start rays outside the surface */
    float eps = grid->cell_size * 0.6f;

    for (int tv = 0; tv < tile_size; tv++) {
        for (int tu = 0; tu < tile_size; tu++) {
            /* Texel center in [0,1] space, offset by 0.5 texel */
            float fu = ((float)tu + 0.5f) / (float)tile_size;
            float fv = ((float)tv + 0.5f) / (float)tile_size;

            /* World-space position on face */
            float wx = face_origin[0] + face_u_axis[0] * fu + face_v_axis[0] * fv;
            float wy = face_origin[1] + face_u_axis[1] * fu + face_v_axis[1] * fv;
            float wz = face_origin[2] + face_u_axis[2] * fu + face_v_axis[2] * fv;

            /* Offset slightly along normal to avoid self-intersection */
            float ox = wx + face_normal[0] * eps;
            float oy = wy + face_normal[1] * eps;
            float oz = wz + face_normal[2] * eps;

            /* Cast hemisphere rays */
            int unoccluded = 0;
            for (int ri = 0; ri < AO_RAY_COUNT; ri++) {
                /* Rotate hemisphere direction from z-up to face normal */
                float lx = s_hemi_dirs[ri].x;
                float ly = s_hemi_dirs[ri].y;
                float lz = s_hemi_dirs[ri].z;

                float dx = tan1[0] * lx + tan2[0] * ly + face_normal[0] * lz;
                float dy = tan1[1] * lx + tan2[1] * ly + face_normal[1] * lz;
                float dz = tan1[2] * lx + tan2[2] * ly + face_normal[2] * lz;

                if (!ray_hits_occupancy(grid, ox, oy, oz, dx, dy, dz, AO_MAX_STEPS)) {
                    unoccluded++;
                }
            }

            /* AO value: 0 = fully occluded, 255 = fully lit */
            float ao = (float)unoccluded / (float)AO_RAY_COUNT;
            out_texels[tv * tile_size + tu] = (uint8_t)(ao * 255.0f + 0.5f);
        }
    }
}
