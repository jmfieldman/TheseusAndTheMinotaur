#ifndef RENDERER_H
#define RENDERER_H

#include <glad/gl.h>
#include "engine/utils.h"

/* Initialize the renderer (shared resources, shaders, etc.) */
void renderer_init(void);

/* Shutdown and release renderer resources. */
void renderer_shutdown(void);

/* Begin a new frame (set viewport, clear). */
void renderer_begin_frame(void);

/* End the frame (flush any batched draws). */
void renderer_end_frame(void);

/* Clear the screen to a color. */
void renderer_clear(Color color);

/* Get the current viewport dimensions in pixels. */
void renderer_get_viewport(int* w, int* h);

/* Get the UI shader program and projection matrix (ortho or perspective). */
GLuint renderer_get_ui_shader(void);
const float* renderer_get_projection_matrix(void);

/* Get the textured UI shader program (for text rendering). */
GLuint renderer_get_ui_tex_shader(void);

/* Get the shared quad VAO (unit quad 0,0 to 1,1). */
GLuint renderer_get_quad_vao(void);

/* Get the voxel shader program (position + normal + color → lit output). */
GLuint renderer_get_voxel_shader(void);

#endif /* RENDERER_H */
