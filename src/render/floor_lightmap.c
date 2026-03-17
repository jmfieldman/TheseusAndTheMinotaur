#include "render/floor_lightmap.h"
#include "engine/utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- Gaussian blur ---------- */

/*
 * Separable Gaussian blur (horizontal then vertical pass).
 * Operates on a float buffer in-place via a temp buffer.
 */
static void gaussian_blur(float* data, int width, int height, float radius) {
    if (radius < 0.5f) return;

    /* Compute kernel — we use a discrete approximation with kernel_size = ceil(radius*3)*2+1 */
    int half_k = (int)ceilf(radius * 2.0f);
    if (half_k < 1) half_k = 1;
    if (half_k > 32) half_k = 32;
    int kernel_size = half_k * 2 + 1;

    float* kernel = (float*)malloc((size_t)kernel_size * sizeof(float));
    float sigma = radius;
    float sum = 0.0f;
    for (int i = 0; i < kernel_size; i++) {
        float x = (float)(i - half_k);
        kernel[i] = expf(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    /* Normalize */
    for (int i = 0; i < kernel_size; i++) kernel[i] /= sum;

    float* temp = (float*)malloc((size_t)width * (size_t)height * sizeof(float));

    /* Horizontal pass */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float val = 0.0f;
            for (int k = -half_k; k <= half_k; k++) {
                int sx = x + k;
                if (sx < 0) sx = 0;
                if (sx >= width) sx = width - 1;
                val += data[y * width + sx] * kernel[k + half_k];
            }
            temp[y * width + x] = val;
        }
    }

    /* Vertical pass */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float val = 0.0f;
            for (int k = -half_k; k <= half_k; k++) {
                int sy = y + k;
                if (sy < 0) sy = 0;
                if (sy >= height) sy = height - 1;
                val += temp[sy * width + x] * kernel[k + half_k];
            }
            data[y * width + x] = val;
        }
    }

    free(temp);
    free(kernel);
}

/* ---------- Public API ---------- */

void floor_lightmap_generate(FloorLightmap* out,
                              const VoxelBox* boxes, int box_count,
                              int cols, int rows,
                              const FloorShadowConfig* cfg) {
    memset(out, 0, sizeof(FloorLightmap));

    int resolution = cfg->shadow_resolution;
    if (resolution < 4) resolution = 4;
    if (resolution > 64) resolution = 64;

    int tex_w = cols * resolution;
    int tex_h = rows * resolution;

    /* World-space bounds: the floor spans [0, cols] x [0, rows] */
    float origin_x = 0.0f;
    float origin_z = 0.0f;
    float extent_x = (float)cols;
    float extent_z = (float)rows;

    /* Allocate float shadow buffer (0 = fully lit, 1 = fully occluded) */
    float* shadow = (float*)calloc((size_t)tex_w * (size_t)tex_h, sizeof(float));
    if (!shadow) {
        LOG_ERROR("floor_lightmap: failed to allocate shadow buffer (%dx%d)", tex_w, tex_h);
        return;
    }

    /* Project each non-floor box's XZ footprint as a shadow */
    for (int i = 0; i < box_count; i++) {
        const VoxelBox* box = &boxes[i];

        /* Skip floor boxes (they don't cast shadows on themselves) */
        if (box->ao_mode == AO_MODE_LIGHTMAP) continue;
        /* Skip occluder-only boxes (handled separately if needed) */
        if (box->occluder_only) continue;
        /* Skip boxes at or below floor level (no shadow contribution) */
        if (box->sy < 0.01f) continue;

        /* Shadow weight based on box height — taller = darker */
        float weight = fminf(box->sy / 0.30f, 1.0f) * cfg->shadow_intensity;
        if (weight < 0.001f) continue;

        /* Compute XZ footprint with absolute padding per side, then offset.
         * Padding is added uniformly so shadows extend the same distance
         * from any wall edge regardless of wall length. */
        float foot_x0 = box->x - cfg->shadow_padding + cfg->shadow_offset_x;
        float foot_z0 = box->z - cfg->shadow_padding + cfg->shadow_offset_z;
        float foot_x1 = box->x + box->sx + cfg->shadow_padding + cfg->shadow_offset_x;
        float foot_z1 = box->z + box->sz + cfg->shadow_padding + cfg->shadow_offset_z;

        /* Convert to texel coordinates */
        int tx0 = (int)floorf((foot_x0 - origin_x) / extent_x * (float)tex_w);
        int tz0 = (int)floorf((foot_z0 - origin_z) / extent_z * (float)tex_h);
        int tx1 = (int)ceilf((foot_x1 - origin_x) / extent_x * (float)tex_w);
        int tz1 = (int)ceilf((foot_z1 - origin_z) / extent_z * (float)tex_h);

        /* Clamp to texture bounds */
        if (tx0 < 0) tx0 = 0;
        if (tz0 < 0) tz0 = 0;
        if (tx1 > tex_w) tx1 = tex_w;
        if (tz1 > tex_h) tz1 = tex_h;

        /* Fill shadow footprint (darkest wins) */
        for (int ty = tz0; ty < tz1; ty++) {
            for (int tx = tx0; tx < tx1; tx++) {
                float* s = &shadow[ty * tex_w + tx];
                if (weight > *s) *s = weight;
            }
        }
    }

    /* Gaussian blur for soft shadow edges.
     * Actual blur = shadow_softness * shadow_blur_radius (the universal
     * softness knob scales the max blur radius). */
    float actual_blur = cfg->shadow_softness * cfg->shadow_blur_radius;
    gaussian_blur(shadow, tex_w, tex_h, actual_blur);

    /* Convert shadow to "lit" space: 1.0 = fully lit, 0.0 = fully occluded.
     * The lightmap is pure shadow data — surface texture detail is handled
     * procedurally in the fragment shader. */
    for (int i = 0; i < tex_w * tex_h; i++) {
        shadow[i] = 1.0f - shadow[i];
    }

    /* Convert to uint8 */
    uint8_t* texels = (uint8_t*)malloc((size_t)tex_w * (size_t)tex_h);
    if (!texels) {
        LOG_ERROR("floor_lightmap: failed to allocate texel buffer");
        free(shadow);
        return;
    }

    for (int i = 0; i < tex_w * tex_h; i++) {
        float v = shadow[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        texels[i] = (uint8_t)(v * 255.0f + 0.5f);
    }

    free(shadow);

    /* Upload as GL_R8 texture */
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, tex_w, tex_h, 0,
                 GL_RED, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(texels);

    /* Fill output */
    out->texture  = tex;
    out->width    = tex_w;
    out->height   = tex_h;
    out->origin_x = origin_x;
    out->origin_z = origin_z;
    out->extent_x = extent_x;
    out->extent_z = extent_z;

    LOG_DEBUG("floor_lightmap: generated %dx%d lightmap for %dx%d grid", tex_w, tex_h, cols, rows);
}

void floor_lightmap_destroy(FloorLightmap* lm) {
    if (lm->texture) {
        glDeleteTextures(1, &lm->texture);
    }
    memset(lm, 0, sizeof(FloorLightmap));
}
