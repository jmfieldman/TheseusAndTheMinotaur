#include "render/renderer.h"
#include "render/shader.h"
#include "engine/engine.h"

#include <string.h>

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

/* ---------- State ---------- */

static struct {
    GLuint ui_shader;
    GLuint ui_tex_shader;
    GLuint quad_vao;
    GLuint quad_vbo;
    float  ortho[16];
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

    if (!s_renderer.ui_shader || !s_renderer.ui_tex_shader) {
        LOG_ERROR("Failed to compile UI shaders");
    }

    LOG_INFO("Renderer initialized");
}

void renderer_shutdown(void) {
    if (s_renderer.ui_shader)     glDeleteProgram(s_renderer.ui_shader);
    if (s_renderer.ui_tex_shader) glDeleteProgram(s_renderer.ui_tex_shader);
    if (s_renderer.quad_vao)      glDeleteVertexArrays(1, &s_renderer.quad_vao);
    if (s_renderer.quad_vbo)      glDeleteBuffers(1, &s_renderer.quad_vbo);
    memset(&s_renderer, 0, sizeof(s_renderer));
    LOG_INFO("Renderer shut down");
}

void renderer_begin_frame(void) {
    int w = g_engine.window_width;
    int h = g_engine.window_height;

    glViewport(0, 0, w, h);

    /* Build ortho: screen-space pixels, origin at top-left */
    build_ortho(s_renderer.ortho, 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);

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

const float* renderer_get_ortho_matrix(void) {
    return s_renderer.ortho;
}

GLuint renderer_get_ui_tex_shader(void) {
    return s_renderer.ui_tex_shader;
}

GLuint renderer_get_quad_vao(void) {
    return s_renderer.quad_vao;
}
