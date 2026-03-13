#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>

/*
 * Isometric-style diorama camera.
 *
 * Uses orthographic projection by default, tilted at an isometric angle
 * to give a 3D view of the voxel diorama. The camera looks down at the
 * scene from an elevated angle.
 *
 * The camera produces a combined view-projection matrix (column-major float[16])
 * suitable for passing directly to the voxel shader's u_vp uniform.
 *
 * Coordinate system:
 *   X = right on screen
 *   Y = up in world (height of voxels)
 *   Z = toward viewer
 *
 * The isometric view rotates the scene around Y, then tilts down.
 * Default angles: yaw ~45°, pitch ~30° (classic isometric feel).
 */

typedef struct {
    /* Camera orientation */
    float yaw;           /* rotation around Y axis (degrees) */
    float pitch;         /* tilt angle from horizontal (degrees, positive = looking down) */

    /* Target point the camera looks at (world space) */
    float target[3];

    /* Orthographic extents (half-width of the view in world units) */
    float ortho_size;

    /* Near/far planes */
    float near_z;
    float far_z;

    /* Output matrices (column-major float[16]) */
    float view[16];
    float proj[16];
    float view_proj[16];

    /* Viewport aspect ratio */
    float aspect;
} DioramaCamera;

/* Initialize camera with default isometric settings.
 * grid_cols/grid_rows: level dimensions for sizing the view. */
void diorama_camera_init(DioramaCamera* cam, int grid_cols, int grid_rows);

/* Set camera orientation. */
void diorama_camera_set_angles(DioramaCamera* cam, float yaw, float pitch);

/* Set the point the camera looks at (center of the diorama). */
void diorama_camera_set_target(DioramaCamera* cam, float x, float y, float z);

/* Set the orthographic view size (half-width in world units). */
void diorama_camera_set_size(DioramaCamera* cam, float size);

/* Update the camera matrices. Call after changing any parameters.
 * viewport_w/h: screen pixel dimensions of the render area. */
void diorama_camera_update(DioramaCamera* cam, int viewport_w, int viewport_h);

/* Get the combined view-projection matrix (column-major). */
const float* diorama_camera_get_vp(const DioramaCamera* cam);

/* Get the view matrix alone (for transforming normals etc.). */
const float* diorama_camera_get_view(const DioramaCamera* cam);

#endif /* CAMERA_H */
