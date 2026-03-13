#ifndef LIGHTING_H
#define LIGHTING_H

#include <glad/gl.h>
#include <stdbool.h>

/*
 * Simple lighting system for voxel rendering.
 *
 * Supports:
 *   - One directional light (sun/key light)
 *   - Up to N point lights (lanterns, effects)
 *
 * Lighting is passed as uniforms to the voxel shader:
 *   u_light_dir      vec3    Normalized direction TO the light (world space)
 *   u_light_color    vec3    Directional light color (HDR ok)
 *   u_ambient_color  vec3    Ambient light color
 *   u_point_lights   int     Number of active point lights
 *   u_point_pos[i]   vec3    Point light position (world space)
 *   u_point_color[i] vec3    Point light color
 *   u_point_radius[i] float  Point light attenuation radius
 */

#define LIGHTING_MAX_POINT_LIGHTS 8

typedef struct {
    float pos[3];
    float color[3];
    float radius;
} PointLight;

typedef struct {
    /* Directional light */
    float dir[3];          /* direction TO the light (normalized) */
    float dir_color[3];    /* directional light color */

    /* Ambient */
    float ambient[3];      /* ambient light color */

    /* Point lights */
    PointLight points[LIGHTING_MAX_POINT_LIGHTS];
    int        point_count;
} LightingState;

/* Initialize with sensible default lighting (top-down sun + warm ambient). */
void lighting_init(LightingState* state);

/* Set the directional light direction (will be normalized) and color. */
void lighting_set_directional(LightingState* state,
                               float dx, float dy, float dz,
                               float r, float g, float b);

/* Set ambient light color. */
void lighting_set_ambient(LightingState* state, float r, float g, float b);

/* Clear all point lights. */
void lighting_clear_points(LightingState* state);

/* Add a point light. Returns false if max reached. */
bool lighting_add_point(LightingState* state,
                         float x, float y, float z,
                         float r, float g, float b,
                         float radius);

/* Upload lighting uniforms to the given shader program.
 * The shader must be already bound (glUseProgram). */
void lighting_apply(const LightingState* state, GLuint shader);

#endif /* LIGHTING_H */
