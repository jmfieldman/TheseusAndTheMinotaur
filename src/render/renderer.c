#include "render/renderer.h"
#include "render/shader.h"
#include "engine/engine.h"
#include "data/settings.h"

#include <string.h>
#include <math.h>

/* ---------- Shader sources ---------- */

static const char* s_ui_vert_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "uniform mat4 u_projection;\n"
    "uniform vec4 u_rect;\n"  /* x, y, w, h */
    "void main() {\n"
    "    vec2 pos = u_rect.xy + a_pos * u_rect.zw;\n"
    "    gl_Position = u_projection * vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char* s_ui_frag_src =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    FragColor = u_color;\n"
    "}\n";

static const char* s_ui_tex_vert_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "out vec2 v_uv;\n"
    "uniform mat4 u_projection;\n"
    "uniform vec4 u_rect;\n"
    "void main() {\n"
    "    vec2 pos = u_rect.xy + a_pos * u_rect.zw;\n"
    "    v_uv = a_pos;\n"
    "    gl_Position = u_projection * vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char* s_ui_tex_frag_src =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    float a = texture(u_texture, v_uv).r;\n"
    "    FragColor = vec4(u_color.rgb, u_color.a * a);\n"
    "}\n";

/* ---------- Voxel shader sources ---------- */

static const char* s_voxel_vert_src =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "\n"
    "uniform mat4 u_vp;\n"           /* view-projection */
    "uniform mat4 u_model;\n"        /* model transform */
    "\n"
    "out vec3 v_world_pos;\n"
    "out vec3 v_normal;\n"
    "out vec4 v_color;\n"
    "\n"
    "void main() {\n"
    "    vec4 world = u_model * vec4(a_position, 1.0);\n"
    "    v_world_pos = world.xyz;\n"
    "    v_normal = mat3(u_model) * a_normal;\n"
    "    v_color = a_color;\n"
    "    gl_Position = u_vp * world;\n"
    "}\n";

static const char* s_voxel_frag_src =
    "#version 330 core\n"
    "\n"
    "in vec3 v_world_pos;\n"
    "in vec3 v_normal;\n"
    "in vec4 v_color;\n"
    "\n"
    "out vec4 FragColor;\n"
    "\n"
    "/* Directional light */\n"
    "uniform vec4 u_light_dir;\n"      /* xyz = direction TO light */
    "uniform vec4 u_light_color;\n"    /* rgb = color */
    "uniform vec4 u_ambient_color;\n"  /* rgb = ambient */
    "\n"
    "/* Point lights */\n"
    "uniform int  u_point_count;\n"
    "uniform vec4 u_point_pos[8];\n"
    "uniform vec4 u_point_color[8];\n"
    "uniform float u_point_radius[8];\n"
    "\n"
    "void main() {\n"
    "    vec3 N = normalize(v_normal);\n"
    "    vec3 base_color = v_color.rgb;\n"
    "\n"
    "    /* Ambient */\n"
    "    vec3 lit = u_ambient_color.rgb * base_color;\n"
    "\n"
    "    /* Directional diffuse (half-Lambert for softer look) */\n"
    "    float ndl = dot(N, u_light_dir.xyz);\n"
    "    float diffuse = ndl * 0.5 + 0.5;\n"  /* half-Lambert */
    "    lit += u_light_color.rgb * base_color * diffuse;\n"
    "\n"
    "    /* Point lights */\n"
    "    for (int i = 0; i < u_point_count; i++) {\n"
    "        vec3 to_light = u_point_pos[i].xyz - v_world_pos;\n"
    "        float dist = length(to_light);\n"
    "        float atten = max(0.0, 1.0 - dist / u_point_radius[i]);\n"
    "        atten *= atten;\n"  /* quadratic falloff */
    "        float pndl = max(0.0, dot(N, normalize(to_light)));\n"
    "        lit += u_point_color[i].rgb * base_color * pndl * atten;\n"
    "    }\n"
    "\n"
    "    FragColor = vec4(lit, v_color.a);\n"
    "}\n";

/* ---------- State ---------- */

static struct {
    GLuint ui_shader;
    GLuint ui_tex_shader;
    GLuint voxel_shader;
    GLuint quad_vao;
    GLuint quad_vbo;
    float  projection[16];
} s_renderer;

/* Build an orthographic projection matrix (column-major). */
static void build_ortho(float* out, float left, float right,
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

/*
 * Build a combined perspective × view matrix that matches the ortho view at z=0.
 *
 * Camera sits at (w/2, h/2, D) looking at (w/2, h/2, 0), where
 * D = (h/2) / tan(fov/2). At z=0, this covers exactly the same region
 * as the orthographic projection, so all existing 2D content renders
 * pixel-identically. The perspective effect only becomes visible when
 * geometry has varying Z depth (e.g. zoom transitions).
 *
 * The view matrix flips Y to match screen-space convention (Y-down,
 * origin at top-left), matching the ortho matrix behavior.
 */
static void build_perspective_view(float* out, float w, float h, float fov_deg) {
    float fov_rad = fov_deg * ((float)M_PI / 180.0f);
    float half_fov = fov_rad * 0.5f;
    float cam_dist = (h * 0.5f) / tanf(half_fov);

    float aspect = w / h;
    float near_z = 1.0f;
    float far_z  = cam_dist * 2.0f;

    /* ── Perspective matrix P (column-major) ─────────────── */
    float f = 1.0f / tanf(half_fov);
    float P[16];
    memset(P, 0, sizeof(P));
    P[0]  = f / aspect;
    P[5]  = f;
    P[10] = -(far_z + near_z) / (far_z - near_z);
    P[11] = -1.0f;
    P[14] = -(2.0f * far_z * near_z) / (far_z - near_z);

    /* ── View matrix V (column-major) ────────────────────── */
    /*
     * Camera at eye = (w/2, h/2, cam_dist), looking at center = (w/2, h/2, 0).
     * Forward = (0, 0, -1), Right = (1, 0, 0), Up = (0, 1, 0).
     *
     * But our screen convention is Y-down (origin top-left), so we flip
     * the Y axis: Up = (0, -1, 0). This makes the view matrix:
     *
     *   R:  (1,  0, 0)    T: (-w/2)
     *   U:  (0, -1, 0)    T: ( h/2)
     *   F:  (0,  0, 1)    T: (-cam_dist)
     *
     * Column-major layout:
     *   col0 = (Rx, Ux, -Fx, 0) = (1, 0, 0, 0)
     *   col1 = (Ry, Uy, -Fy, 0) = (0, -1, 0, 0)
     *   col2 = (Rz, Uz, -Fz, 0) = (0, 0, -1, 0)
     *   col3 = (-dot(R,eye), -dot(U,eye), dot(F,eye), 1)
     *        = (-w/2, h/2, -cam_dist, 1)
     */
    float V[16];
    memset(V, 0, sizeof(V));
    V[0]  = 1.0f;                    /* col0.x */
    V[5]  = -1.0f;                   /* col1.y (Y flip) */
    V[10] = -1.0f;                   /* col2.z */
    V[12] = -(w * 0.5f);            /* col3.x: -dot(R, eye) */
    V[13] = h * 0.5f;               /* col3.y: -dot(U, eye), U=(0,-1,0) so -(-h/2) = h/2 */
    V[14] = -cam_dist;              /* col3.z: dot(F, eye), but F points at -Z, so -(cam_dist) */
    V[15] = 1.0f;

    /* ── Multiply P × V → out (column-major) ────────────── */
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += P[k * 4 + r] * V[c * 4 + k];
            }
            out[c * 4 + r] = sum;
        }
    }
}

void renderer_init(void) {
    /* Unit quad (0,0) to (1,1) */
    float quad_verts[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };

    glGenVertexArrays(1, &s_renderer.quad_vao);
    glGenBuffers(1, &s_renderer.quad_vbo);

    glBindVertexArray(s_renderer.quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_renderer.quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    /* Compile shaders */
    s_renderer.ui_shader = shader_compile(s_ui_vert_src, s_ui_frag_src);
    s_renderer.ui_tex_shader = shader_compile(s_ui_tex_vert_src, s_ui_tex_frag_src);
    s_renderer.voxel_shader = shader_compile(s_voxel_vert_src, s_voxel_frag_src);

    if (!s_renderer.ui_shader || !s_renderer.ui_tex_shader) {
        LOG_ERROR("Failed to compile UI shaders");
    }
    if (!s_renderer.voxel_shader) {
        LOG_ERROR("Failed to compile voxel shader");
    }

    LOG_INFO("Renderer initialized");
}

void renderer_shutdown(void) {
    if (s_renderer.ui_shader)     glDeleteProgram(s_renderer.ui_shader);
    if (s_renderer.ui_tex_shader) glDeleteProgram(s_renderer.ui_tex_shader);
    if (s_renderer.voxel_shader)  glDeleteProgram(s_renderer.voxel_shader);
    if (s_renderer.quad_vao)      glDeleteVertexArrays(1, &s_renderer.quad_vao);
    if (s_renderer.quad_vbo)      glDeleteBuffers(1, &s_renderer.quad_vbo);
    memset(&s_renderer, 0, sizeof(s_renderer));
    LOG_INFO("Renderer shut down");
}

void renderer_begin_frame(void) {
    int w = g_engine.window_width;
    int h = g_engine.window_height;

    glViewport(0, 0, w, h);

    /* Build projection matrix based on camera mode */
    if (g_settings.camera_perspective && h > 0) {
        build_perspective_view(s_renderer.projection, (float)w, (float)h,
                               g_settings.camera_fov);
    } else {
        /* Ortho: screen-space pixels, origin at top-left */
        build_ortho(s_renderer.projection, 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    }

    /*
     * Depth test stays disabled for now — all current geometry is at z=0,
     * so GL_LESS would reject everything after the first draw per pixel.
     * Enable depth test later when we have 3D geometry with varying Z.
     */
    glDisable(GL_DEPTH_TEST);

    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_end_frame(void) {
    /* Nothing to flush yet */
}

void renderer_clear(Color color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_get_viewport(int* w, int* h) {
    *w = g_engine.window_width;
    *h = g_engine.window_height;
}

GLuint renderer_get_ui_shader(void) {
    return s_renderer.ui_shader;
}

const float* renderer_get_projection_matrix(void) {
    return s_renderer.projection;
}

GLuint renderer_get_ui_tex_shader(void) {
    return s_renderer.ui_tex_shader;
}

GLuint renderer_get_quad_vao(void) {
    return s_renderer.quad_vao;
}

GLuint renderer_get_voxel_shader(void) {
    return s_renderer.voxel_shader;
}
