#include "render/ui_draw.h"
#include "render/renderer.h"
#include "render/shader.h"

#include <glad/gl.h>
#include <math.h>

void ui_draw_rect(float x, float y, float w, float h, Color color) {
    GLuint shader = renderer_get_ui_shader();
    shader_use(shader);
    shader_set_mat4(shader, "u_projection", renderer_get_ortho_matrix());
    shader_set_vec4(shader, "u_rect", x, y, w, h);
    shader_set_vec4(shader, "u_color", color.r, color.g, color.b, color.a);

    glBindVertexArray(renderer_get_quad_vao());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void ui_draw_rect_rounded(float x, float y, float w, float h,
                          float radius, Color color) {
    if (radius < 1.0f) {
        ui_draw_rect(x, y, w, h, color);
        return;
    }

    /* Clamp radius */
    float max_r = fminf(w, h) * 0.5f;
    if (radius > max_r) radius = max_r;

    /*
     * Draw rounded rect as 5 rects (cross shape) + 4 corner arcs.
     * This is a simple approach; could optimize with a custom VBO later.
     */

    /* Center cross */
    ui_draw_rect(x + radius, y, w - 2*radius, h, color);         /* center column */
    ui_draw_rect(x, y + radius, radius, h - 2*radius, color);    /* left strip */
    ui_draw_rect(x + w - radius, y + radius, radius, h - 2*radius, color); /* right strip */

    /* Corner arcs using a fan of triangles */
    GLuint shader = renderer_get_ui_shader();
    shader_use(shader);
    shader_set_mat4(shader, "u_projection", renderer_get_ortho_matrix());
    shader_set_vec4(shader, "u_color", color.r, color.g, color.b, color.a);

    /* We'll draw each corner as a triangle fan */
    #define CORNER_SEGMENTS 8

    struct { float cx, cy; float angle_start; } corners[4] = {
        { x + radius,     y + radius,     (float)M_PI },       /* top-left */
        { x + w - radius, y + radius,     (float)M_PI * 1.5f },/* top-right */
        { x + w - radius, y + h - radius, 0.0f },              /* bottom-right */
        { x + radius,     y + h - radius, (float)M_PI * 0.5f },/* bottom-left */
    };

    /* Build a temporary VAO for corner arcs */
    float verts[(CORNER_SEGMENTS + 2) * 2]; /* center + segments + 1 */
    GLuint corner_vao, corner_vbo;
    glGenVertexArrays(1, &corner_vao);
    glGenBuffers(1, &corner_vbo);
    glBindVertexArray(corner_vao);
    glBindBuffer(GL_ARRAY_BUFFER, corner_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    for (int c = 0; c < 4; c++) {
        float cx = corners[c].cx;
        float cy = corners[c].cy;
        float a0 = corners[c].angle_start;

        /* Center vertex */
        verts[0] = cx;
        verts[1] = cy;

        for (int i = 0; i <= CORNER_SEGMENTS; i++) {
            float angle = a0 + ((float)M_PI * 0.5f) * (float)i / (float)CORNER_SEGMENTS;
            verts[(i + 1) * 2 + 0] = cx + cosf(angle) * radius;
            verts[(i + 1) * 2 + 1] = cy + sinf(angle) * radius;
        }

        /* Identity rect transform (we already computed absolute positions) */
        shader_set_vec4(shader, "u_rect", 0.0f, 0.0f, 1.0f, 1.0f);

        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * (CORNER_SEGMENTS + 2) * 2,
                     verts, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, CORNER_SEGMENTS + 2);
    }

    glBindVertexArray(0);
    glDeleteBuffers(1, &corner_vbo);
    glDeleteVertexArrays(1, &corner_vao);
    #undef CORNER_SEGMENTS
}
