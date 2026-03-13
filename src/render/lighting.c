#include "render/lighting.h"
#include "render/shader.h"
#include "engine/utils.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

/* ---------- Init ---------- */

void lighting_init(LightingState* state) {
    memset(state, 0, sizeof(LightingState));

    /* Default: warm directional light from upper-right-front */
    float dx = -0.4f, dy = 0.8f, dz = 0.5f;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    state->dir[0] = dx / len;
    state->dir[1] = dy / len;
    state->dir[2] = dz / len;
    state->dir_color[0] = 0.9f;
    state->dir_color[1] = 0.85f;
    state->dir_color[2] = 0.75f;

    /* Soft ambient */
    state->ambient[0] = 0.25f;
    state->ambient[1] = 0.27f;
    state->ambient[2] = 0.30f;
}

/* ---------- Configuration ---------- */

void lighting_set_directional(LightingState* state,
                               float dx, float dy, float dz,
                               float r, float g, float b) {
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 1e-6f) len = 1.0f;
    state->dir[0] = dx / len;
    state->dir[1] = dy / len;
    state->dir[2] = dz / len;
    state->dir_color[0] = r;
    state->dir_color[1] = g;
    state->dir_color[2] = b;
}

void lighting_set_ambient(LightingState* state, float r, float g, float b) {
    state->ambient[0] = r;
    state->ambient[1] = g;
    state->ambient[2] = b;
}

void lighting_clear_points(LightingState* state) {
    state->point_count = 0;
}

bool lighting_add_point(LightingState* state,
                         float x, float y, float z,
                         float r, float g, float b,
                         float radius) {
    if (state->point_count >= LIGHTING_MAX_POINT_LIGHTS) {
        return false;
    }
    PointLight* pl = &state->points[state->point_count++];
    pl->pos[0] = x; pl->pos[1] = y; pl->pos[2] = z;
    pl->color[0] = r; pl->color[1] = g; pl->color[2] = b;
    pl->radius = radius;
    return true;
}

/* ---------- Upload uniforms ---------- */

void lighting_apply(const LightingState* state, GLuint shader) {
    /* Directional light */
    shader_set_vec4(shader, "u_light_dir",
                    state->dir[0], state->dir[1], state->dir[2], 0.0f);
    shader_set_vec4(shader, "u_light_color",
                    state->dir_color[0], state->dir_color[1],
                    state->dir_color[2], 1.0f);
    shader_set_vec4(shader, "u_ambient_color",
                    state->ambient[0], state->ambient[1],
                    state->ambient[2], 1.0f);

    /* Point lights */
    shader_set_int(shader, "u_point_count", state->point_count);

    for (int i = 0; i < state->point_count; i++) {
        const PointLight* pl = &state->points[i];
        char name[64];

        snprintf(name, sizeof(name), "u_point_pos[%d]", i);
        shader_set_vec4(shader, name, pl->pos[0], pl->pos[1], pl->pos[2], 0.0f);

        snprintf(name, sizeof(name), "u_point_color[%d]", i);
        shader_set_vec4(shader, name, pl->color[0], pl->color[1], pl->color[2], 1.0f);

        snprintf(name, sizeof(name), "u_point_radius[%d]", i);
        shader_set_float(shader, name, pl->radius);
    }
}
