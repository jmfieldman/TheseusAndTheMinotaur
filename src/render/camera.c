#include "render/camera.h"
#include "engine/utils.h"
#include "data/settings.h"

#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG_TO_RAD(d) ((d) * ((float)M_PI / 180.0f))

/* ---------- Matrix helpers ---------- */

static void mat4_identity(float* out) {
    memset(out, 0, 16 * sizeof(float));
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}

static void mat4_multiply(float* out, const float* a, const float* b) {
    float tmp[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a[k * 4 + r] * b[c * 4 + k];
            }
            tmp[c * 4 + r] = sum;
        }
    }
    memcpy(out, tmp, sizeof(tmp));
}

/* Build an orthographic projection matrix (column-major). */
static void mat4_ortho(float* out,
                        float left, float right,
                        float bottom, float top,
                        float near, float far) {
    memset(out, 0, 16 * sizeof(float));
    out[0]  = 2.0f / (right - left);
    out[5]  = 2.0f / (top - bottom);
    out[10] = -2.0f / (far - near);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(far + near) / (far - near);
    out[15] = 1.0f;
}

/* Build a symmetric perspective projection matrix (column-major). */
static void mat4_perspective(float* out,
                              float fov_rad, float aspect,
                              float near, float far) {
    float f = 1.0f / tanf(fov_rad * 0.5f);
    memset(out, 0, 16 * sizeof(float));
    out[0]  = f / aspect;
    out[5]  = f;
    out[10] = -(far + near) / (far - near);
    out[11] = -1.0f;
    out[14] = -(2.0f * far * near) / (far - near);
}

/*
 * Build a standard lookAt view matrix (column-major).
 *
 * Uses the OpenGL convention: camera looks along -Z in view space.
 *   f = normalize(center - eye)        forward direction
 *   s = normalize(f × world_up)        side (right on screen)
 *   u = s × f                          camera up
 *
 * View matrix:
 *   col0 = (s.x, u.x, -f.x, 0)
 *   col1 = (s.y, u.y, -f.y, 0)
 *   col2 = (s.z, u.z, -f.z, 0)
 *   col3 = (-dot(s,eye), -dot(u,eye), dot(f,eye), 1)
 */
static void build_look_at(float* out,
                            float eye_x, float eye_y, float eye_z,
                            float cx, float cy, float cz) {
    /* Forward = normalize(center - eye) */
    float fx = cx - eye_x;
    float fy = cy - eye_y;
    float fz = cz - eye_z;
    float flen = sqrtf(fx * fx + fy * fy + fz * fz);
    if (flen < 1e-6f) flen = 1.0f;
    fx /= flen; fy /= flen; fz /= flen;

    /* Side = normalize(forward × world_up)  where world_up = (0, 1, 0) */
    float sx = fy * 0.0f - fz * 1.0f;  /* = -fz */
    float sy = fz * 0.0f - fx * 0.0f;  /* = 0 */
    float sz = fx * 1.0f - fy * 0.0f;  /* = fx */
    /* Simplified: s = (-fz, 0, fx) */
    sx = -fz; sy = 0.0f; sz = fx;

    float slen = sqrtf(sx * sx + sz * sz);
    if (slen < 1e-6f) {
        /* Degenerate case: looking straight up/down. Pick an arbitrary side. */
        sx = 1.0f; sz = 0.0f; slen = 1.0f;
    } else {
        sx /= slen; sz /= slen;
    }

    /* Up = side × forward */
    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    /* View matrix (column-major) */
    memset(out, 0, 16 * sizeof(float));
    out[0]  = sx;
    out[1]  = ux;
    out[2]  = -fx;

    out[4]  = sy;
    out[5]  = uy;
    out[6]  = -fy;

    out[8]  = sz;
    out[9]  = uz;
    out[10] = -fz;

    out[12] = -(sx * eye_x + sy * eye_y + sz * eye_z);
    out[13] = -(ux * eye_x + uy * eye_y + uz * eye_z);
    out[14] = (fx * eye_x + fy * eye_y + fz * eye_z);

    out[15] = 1.0f;
}

/* ---------- Public API ---------- */

void diorama_camera_init(DioramaCamera* cam, int grid_cols, int grid_rows) {
    memset(cam, 0, sizeof(DioramaCamera));

    /*
     * Top-down-ish view: yaw=0 so south wall aligns with screen bottom,
     * pitch=65° so we look mostly down but still see wall faces.
     */
    cam->yaw = 0.0f;
    cam->pitch = 65.0f;

    /* Center target on grid (tile_size=1 in world units for now) */
    cam->target[0] = grid_cols * 0.5f;
    cam->target[1] = 0.0f;
    cam->target[2] = grid_rows * 0.5f;

    /* Size the view to fit the grid with some margin */
    float max_dim = (float)(grid_cols > grid_rows ? grid_cols : grid_rows);
    cam->ortho_size = max_dim * 0.7f;

    cam->near_z = 0.1f;
    cam->far_z = 100.0f;
    cam->aspect = 1.0f;

    mat4_identity(cam->view);
    mat4_identity(cam->proj);
    mat4_identity(cam->view_proj);
}

void diorama_camera_set_angles(DioramaCamera* cam, float yaw, float pitch) {
    cam->yaw = yaw;
    cam->pitch = CLAMP(pitch, 5.0f, 85.0f);
}

void diorama_camera_set_target(DioramaCamera* cam, float x, float y, float z) {
    cam->target[0] = x;
    cam->target[1] = y;
    cam->target[2] = z;
}

void diorama_camera_set_size(DioramaCamera* cam, float size) {
    cam->ortho_size = size;
}

void diorama_camera_update(DioramaCamera* cam, int viewport_w, int viewport_h) {
    if (viewport_h == 0) viewport_h = 1;
    cam->aspect = (float)viewport_w / (float)viewport_h;

    /* Compute eye position from yaw, pitch, and distance */
    float yaw = DEG_TO_RAD(cam->yaw);
    float pitch = DEG_TO_RAD(cam->pitch);
    float distance = (cam->far_z - cam->near_z) * 0.4f;

    /* Forward direction (from camera toward target) */
    float fwd_x = cosf(pitch) * sinf(yaw);
    float fwd_y = -sinf(pitch);
    float fwd_z = cosf(pitch) * cosf(yaw);

    /* Eye = target - forward * distance */
    float eye_x = cam->target[0] - fwd_x * distance;
    float eye_y = cam->target[1] - fwd_y * distance;
    float eye_z = cam->target[2] - fwd_z * distance;

    /* View matrix via standard lookAt */
    build_look_at(cam->view,
                  eye_x, eye_y, eye_z,
                  cam->target[0], cam->target[1], cam->target[2]);

    /* Projection: orthographic or perspective based on setting */
    if (g_settings.camera_perspective) {
        float fov_rad = DEG_TO_RAD(g_settings.camera_fov);
        mat4_perspective(cam->proj, fov_rad, cam->aspect,
                         cam->near_z, cam->far_z);
    } else {
        float half_w = cam->ortho_size * cam->aspect;
        float half_h = cam->ortho_size;
        mat4_ortho(cam->proj, -half_w, half_w, -half_h, half_h,
                   cam->near_z, cam->far_z);
    }

    /* Combined VP */
    mat4_multiply(cam->view_proj, cam->proj, cam->view);
}

const float* diorama_camera_get_vp(const DioramaCamera* cam) {
    return cam->view_proj;
}

const float* diorama_camera_get_view(const DioramaCamera* cam) {
    return cam->view;
}
