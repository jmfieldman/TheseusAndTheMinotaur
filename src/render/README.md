# src/render/

OpenGL rendering subsystem. Handles shader management, 2D UI drawing, and text rendering.

## Files

| File                | Purpose |
|---------------------|---------|
| `shader.h / shader.c` | Shader compilation and uniform utilities. `shader_compile()` takes vertex + fragment source strings, returns a linked GLuint program. Uniform setters for mat4, vec4, vec2, float, int. |
| `renderer.h / renderer.c` | Core renderer. Creates shared quad VBO (unit quad 0–1), compiles two built-in shaders (flat color UI + textured UI), and manages the projection matrix. Supports orthographic (default) and perspective projection modes, toggled via `g_settings.camera_perspective`. In perspective mode, builds a combined P×V matrix calibrated so that z=0 geometry renders pixel-identically to ortho. Provides `renderer_begin_frame()` / `renderer_end_frame()`. |
| `ui_draw.h / ui_draw.c` | Immediate-mode 2D drawing. `ui_draw_rect()` for filled rectangles, `ui_draw_rect_rounded()` for rounded corners (triangle fan per corner, 8 segments). All coordinates in screen-space pixels. |
| `text_render.h / text_render.c` | Text rendering via SDL_ttf. Loads a single TTF at 5 sizes (16, 24, 32, 48, 64). Renders text to GL textures with an LRU cache (128 entries). Supports LEFT/CENTER/RIGHT alignment. Uses a single-channel (GL_RED) texture with the alpha from SDL_ttf's blended output. |

## Shaders

Two shader programs are compiled at init, embedded as string constants:

1. **UI shader** — Flat-colored quads. Uniforms: `u_projection` (mat4), `u_rect` (vec4: x,y,w,h), `u_color` (vec4).
2. **UI texture shader** — Textured quads for text glyphs. Same uniforms plus `u_texture` (sampler2D). Samples the red channel as alpha.

## Future

- 3D diorama rendering (voxel meshes, shadow maps, dynamic lighting) will be added here.
- Post-processing (vignette, bloom) will be a separate pass.
