#ifndef SHADER_H
#define SHADER_H

#include <glad/gl.h>

/* Compile vertex + fragment shader source into a linked program.
 * Returns 0 on failure. */
GLuint shader_compile(const char* vert_src, const char* frag_src);

/* Bind a shader program. */
void shader_use(GLuint program);

/* Uniform setters */
void shader_set_mat4(GLuint program, const char* name, const float* mat);
void shader_set_vec4(GLuint program, const char* name, float x, float y, float z, float w);
void shader_set_vec2(GLuint program, const char* name, float x, float y);
void shader_set_float(GLuint program, const char* name, float val);
void shader_set_int(GLuint program, const char* name, int val);

#endif /* SHADER_H */
