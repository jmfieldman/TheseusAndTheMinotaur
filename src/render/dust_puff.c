#include "render/dust_puff.h"
#include "render/shader.h"
#include "engine/utils.h"

#include <glad/gl.h>
#include <stb_image.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- Tuning constants ---------- */

#define DUST_MAX_PARTICLES   64    /* pool size */
#define DUST_SPAWN_COUNT     8     /* particles per stomp */
#define DUST_LIFETIME_MIN    0.35f /* seconds */
#define DUST_LIFETIME_MAX    0.55f /* seconds */
#define DUST_RADIAL_SPEED    2.5f  /* world units/sec outward (high initial burst) */
#define DUST_RISE_SPEED      0.8f  /* world units/sec upward */
#define DUST_SIZE_START      0.06f /* world units (small) */
#define DUST_SIZE_END        0.22f /* world units (expands) */
#define DUST_SPIN_SPEED_MIN  1.5f  /* radians/sec */
#define DUST_SPIN_SPEED_MAX  4.0f  /* radians/sec */
#define DUST_SPAWN_RADIUS    0.15f /* initial ring radius from center */
#define DUST_FADE_START      0.3f  /* fraction of lifetime when fade begins */

/* ---------- Particle data ---------- */

typedef struct {
    float pos[3];       /* world position */
    float vel[3];       /* velocity (world units/sec) */
    float age;          /* elapsed time */
    float lifetime;     /* max age */
    float size_start;   /* initial billboard size */
    float size_end;     /* final billboard size */
    float rotation;     /* current rotation angle (radians) */
    float spin;         /* angular velocity (radians/sec) */
    bool  alive;
} DustParticle;

/* ---------- Shader sources ---------- */

static const char* s_dust_vert_src =
    "#version 330 core\n"
    "\n"
    "/* Per-particle instance data packed into vertex attributes.\n"
    " * We draw 6 vertices (2 triangles) per particle via instancing. */\n"
    "layout(location = 0) in vec3 a_center;    /* world position */\n"
    "layout(location = 1) in float a_size;     /* billboard half-extent */\n"
    "layout(location = 2) in float a_opacity;  /* 0-1 fade */\n"
    "layout(location = 3) in float a_rotation; /* radians */\n"
    "\n"
    "uniform mat4 u_vp;\n"
    "uniform vec3 u_cam_right;\n"
    "uniform vec3 u_cam_up;\n"
    "\n"
    "out vec2 v_uv;\n"
    "out float v_opacity;\n"
    "\n"
    "void main() {\n"
    "    /* 6 vertices = 2 triangles forming a quad.\n"
    "     * gl_VertexID % 6 selects the corner. */\n"
    "    int vid = gl_VertexID % 6;\n"
    "    vec2 corner;\n"
    "    if      (vid == 0) corner = vec2(-1.0, -1.0);\n"
    "    else if (vid == 1) corner = vec2( 1.0, -1.0);\n"
    "    else if (vid == 2) corner = vec2( 1.0,  1.0);\n"
    "    else if (vid == 3) corner = vec2(-1.0, -1.0);\n"
    "    else if (vid == 4) corner = vec2( 1.0,  1.0);\n"
    "    else               corner = vec2(-1.0,  1.0);\n"
    "\n"
    "    /* Apply rotation to corner */\n"
    "    float c = cos(a_rotation);\n"
    "    float s = sin(a_rotation);\n"
    "    vec2 rc = vec2(c * corner.x - s * corner.y,\n"
    "                   s * corner.x + c * corner.y);\n"
    "\n"
    "    /* Billboard: expand in camera-aligned plane */\n"
    "    vec3 world_pos = a_center\n"
    "        + u_cam_right * (rc.x * a_size)\n"
    "        + u_cam_up    * (rc.y * a_size);\n"
    "\n"
    "    gl_Position = u_vp * vec4(world_pos, 1.0);\n"
    "    v_uv = corner * 0.5 + 0.5;\n"
    "    v_opacity = a_opacity;\n"
    "}\n";

static const char* s_dust_frag_src =
    "#version 330 core\n"
    "\n"
    "in vec2 v_uv;\n"
    "in float v_opacity;\n"
    "out vec4 FragColor;\n"
    "\n"
    "uniform sampler2D u_dust_tex;\n"
    "\n"
    "void main() {\n"
    "    vec4 tex = texture(u_dust_tex, v_uv);\n"
    "    FragColor = vec4(tex.rgb, tex.a * v_opacity);\n"
    "}\n";

/* ---------- GL state ---------- */

static struct {
    GLuint shader;
    GLuint vao;
    GLuint vbo;        /* per-particle instance data */
    GLuint texture;    /* dust.png texture */
    DustParticle particles[DUST_MAX_PARTICLES];
    int active_count;  /* number of live particles (for draw count) */
} s_dust;

/* Per-particle GPU data (matches vertex attributes) */
typedef struct {
    float center[3];   /* a_center */
    float size;        /* a_size */
    float opacity;     /* a_opacity */
    float rotation;    /* a_rotation */
} DustGPUData;

/* ---------- Random helpers ---------- */

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float randf_range(float lo, float hi) {
    return lo + randf() * (hi - lo);
}

/* ---------- Texture loading ---------- */

static GLuint load_dust_texture(const char* path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);  /* force RGBA */
    if (!data) {
        LOG_ERROR("Failed to load dust texture: %s", path);
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    LOG_INFO("Dust texture loaded: %s (%dx%d, %d channels)", path, w, h, channels);
    return tex;
}

/* ---------- Public API ---------- */

void dust_puff_init(void) {
    memset(&s_dust, 0, sizeof(s_dust));

    s_dust.shader = shader_compile(s_dust_vert_src, s_dust_frag_src);
    if (!s_dust.shader) {
        LOG_ERROR("Failed to compile dust puff shader");
        return;
    }

    /* Set texture uniform binding */
    shader_use(s_dust.shader);
    shader_set_int(s_dust.shader, "u_dust_tex", 0);
    glUseProgram(0);

    /* Load dust texture */
    s_dust.texture = load_dust_texture("assets/textures/dust.png");

    /* Create VAO with per-instance vertex attributes.
     * We use instanced rendering: 6 vertices per instance (2 triangles),
     * with instance data providing center/size/opacity/rotation. */
    glGenVertexArrays(1, &s_dust.vao);
    glGenBuffers(1, &s_dust.vbo);

    glBindVertexArray(s_dust.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_dust.vbo);
    glBufferData(GL_ARRAY_BUFFER, DUST_MAX_PARTICLES * sizeof(DustGPUData),
                 NULL, GL_DYNAMIC_DRAW);

    /* layout(location=0) vec3 a_center — offset 0 */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DustGPUData),
                          (void*)offsetof(DustGPUData, center));
    glVertexAttribDivisor(0, 1);  /* per instance */

    /* layout(location=1) float a_size — offset 12 */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(DustGPUData),
                          (void*)offsetof(DustGPUData, size));
    glVertexAttribDivisor(1, 1);

    /* layout(location=2) float a_opacity — offset 16 */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(DustGPUData),
                          (void*)offsetof(DustGPUData, opacity));
    glVertexAttribDivisor(2, 1);

    /* layout(location=3) float a_rotation — offset 20 */
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(DustGPUData),
                          (void*)offsetof(DustGPUData, rotation));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    LOG_INFO("Dust puff system initialized");
}

void dust_puff_shutdown(void) {
    if (s_dust.shader)  glDeleteProgram(s_dust.shader);
    if (s_dust.vao)     glDeleteVertexArrays(1, &s_dust.vao);
    if (s_dust.vbo)     glDeleteBuffers(1, &s_dust.vbo);
    if (s_dust.texture) glDeleteTextures(1, &s_dust.texture);
    memset(&s_dust, 0, sizeof(s_dust));
}

void dust_puff_spawn(float x, float y, float z) {
    for (int i = 0; i < DUST_SPAWN_COUNT; i++) {
        /* Find a free slot */
        int slot = -1;
        for (int j = 0; j < DUST_MAX_PARTICLES; j++) {
            if (!s_dust.particles[j].alive) {
                slot = j;
                break;
            }
        }
        if (slot < 0) break;  /* pool full */

        DustParticle* p = &s_dust.particles[slot];
        p->alive = true;
        p->age = 0.0f;
        p->lifetime = randf_range(DUST_LIFETIME_MIN, DUST_LIFETIME_MAX);

        /* Random angle around the ring */
        float angle = randf() * 2.0f * (float)M_PI;
        float ca = cosf(angle);
        float sa = sinf(angle);

        /* Initial position: slightly offset from center */
        float spawn_r = DUST_SPAWN_RADIUS * randf_range(0.5f, 1.0f);
        p->pos[0] = x + ca * spawn_r;
        p->pos[1] = y + 0.02f;  /* just above ground */
        p->pos[2] = z + sa * spawn_r;

        /* Velocity: radially outward + upward */
        float speed = DUST_RADIAL_SPEED * randf_range(0.7f, 1.3f);
        p->vel[0] = ca * speed;
        p->vel[1] = DUST_RISE_SPEED * randf_range(0.6f, 1.4f);
        p->vel[2] = sa * speed;

        /* Size ramp */
        float scale_var = randf_range(0.8f, 1.2f);
        p->size_start = DUST_SIZE_START * scale_var;
        p->size_end   = DUST_SIZE_END * scale_var;

        /* Rotation */
        p->rotation = randf() * 2.0f * (float)M_PI;
        p->spin = randf_range(DUST_SPIN_SPEED_MIN, DUST_SPIN_SPEED_MAX);
        if (randf() > 0.5f) p->spin = -p->spin;
    }
}

void dust_puff_update(float dt) {
    for (int i = 0; i < DUST_MAX_PARTICLES; i++) {
        DustParticle* p = &s_dust.particles[i];
        if (!p->alive) continue;

        p->age += dt;
        if (p->age >= p->lifetime) {
            p->alive = false;
            continue;
        }

        /* Integrate position */
        p->pos[0] += p->vel[0] * dt;
        p->pos[1] += p->vel[1] * dt;
        p->pos[2] += p->vel[2] * dt;

        /* Exponential drag for ease-out: starts fast, slows down quickly.
         * drag_factor = e^(-k*dt) ≈ continuous exponential decay.
         * k=14: max radial displacement ≈ v0/k = 2.5/14 ≈ 0.18 units,
         * keeping puffs contained within the minotaur's tile. */
        float drag = expf(-14.0f * dt);
        p->vel[0] *= drag;
        p->vel[2] *= drag;

        /* Same ease-out for upward velocity (softer so they still float up) */
        p->vel[1] *= expf(-8.0f * dt);

        /* Spin */
        p->rotation += p->spin * dt;
    }
}

bool dust_puff_render(const float* vp, const float* view) {
    if (!s_dust.shader || !s_dust.texture) return false;

    /* Collect live particles into GPU buffer */
    DustGPUData gpu_data[DUST_MAX_PARTICLES];
    int count = 0;

    for (int i = 0; i < DUST_MAX_PARTICLES; i++) {
        DustParticle* p = &s_dust.particles[i];
        if (!p->alive) continue;

        float t = p->age / p->lifetime;  /* 0→1 normalized age */

        /* Size: lerp from start to end */
        float size = p->size_start + (p->size_end - p->size_start) * t;

        /* Opacity: full until DUST_FADE_START, then fade to 0 */
        float opacity = 1.0f;
        if (t > DUST_FADE_START) {
            opacity = 1.0f - (t - DUST_FADE_START) / (1.0f - DUST_FADE_START);
        }
        /* Quick fade-in over first 10% */
        if (t < 0.1f) {
            opacity *= t / 0.1f;
        }

        gpu_data[count].center[0] = p->pos[0];
        gpu_data[count].center[1] = p->pos[1];
        gpu_data[count].center[2] = p->pos[2];
        gpu_data[count].size = size;
        gpu_data[count].opacity = opacity;
        gpu_data[count].rotation = p->rotation;
        count++;
    }

    if (count == 0) return false;

    s_dust.active_count = count;

    /* Upload instance data */
    glBindBuffer(GL_ARRAY_BUFFER, s_dust.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(DustGPUData), gpu_data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Extract camera right and up vectors from the view matrix.
     * View matrix (column-major): col0 = right, col1 = up, col2 = -forward.
     * But we need world-space vectors, so we read the ROWS of the 3x3 part. */
    float cam_right[3] = { view[0], view[4], view[8]  };
    float cam_up[3]    = { view[1], view[5], view[9]  };

    /* Draw with blending, depth read (no write) */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);  /* don't write depth — particles are translucent */

    shader_use(s_dust.shader);
    shader_set_mat4(s_dust.shader, "u_vp", vp);
    shader_set_vec3(s_dust.shader, "u_cam_right", cam_right[0], cam_right[1], cam_right[2]);
    shader_set_vec3(s_dust.shader, "u_cam_up", cam_up[0], cam_up[1], cam_up[2]);

    /* Bind dust texture to unit 0 */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_dust.texture);

    glBindVertexArray(s_dust.vao);
    /* 6 vertices per particle (2 triangles), instanced */
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
    glBindVertexArray(0);

    /* Restore state */
    glDepthMask(GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    return true;
}

bool dust_puff_is_active(void) {
    for (int i = 0; i < DUST_MAX_PARTICLES; i++) {
        if (s_dust.particles[i].alive) return true;
    }
    return false;
}
