# src/render/

OpenGL rendering subsystem. Handles shader management, 2D UI drawing, text rendering, and 3D voxel diorama rendering.

## Files

| File                | Purpose |
|---------------------|---------|
| `shader.h / shader.c` | Shader compilation and uniform utilities. `shader_compile()` takes vertex + fragment source strings, returns a linked GLuint program. Uniform setters for mat4, vec4, vec2, float, int. |
| `renderer.h / renderer.c` | Core renderer. Creates shared quad VBO (unit quad 0–1), compiles three built-in shaders (flat color UI, textured UI, voxel), and manages the projection matrix. Supports orthographic (default) and perspective projection modes, toggled via `g_settings.camera_perspective`. In perspective mode, builds a combined P×V matrix calibrated so that z=0 geometry renders pixel-identically to ortho. Provides `renderer_begin_frame()` / `renderer_end_frame()`. |
| `ui_draw.h / ui_draw.c` | Immediate-mode 2D drawing. `ui_draw_rect()` for filled rectangles, `ui_draw_rect_rounded()` for rounded corners (triangle fan per corner, 8 segments). All coordinates in screen-space pixels. |
| `text_render.h / text_render.c` | Text rendering via SDL_ttf. Loads a single TTF at 5 sizes (16, 24, 32, 48, 64). Renders text to GL textures with an LRU cache (128 entries). Supports LEFT/CENTER/RIGHT alignment. Uses a single-channel (GL_RED) texture with the alpha from SDL_ttf's blended output. |
| `camera.h / camera.c` | Isometric-style diorama camera. Orthographic projection with configurable yaw/pitch angles, target point, and view size. Produces a combined view-projection matrix for the voxel shader. |
| `voxel_mesh.h / voxel_mesh.c` | Freeform box mesh builder. Accumulates axis-aligned boxes at arbitrary positions and dimensions, then builds a single VBO with hidden face culling and baked ambient occlusion. Vertex layout: position (vec3) + normal (vec3) + color (vec4). Uses `OccupancyGrid` during build for culling and AO. |
| `occupancy_grid.h / occupancy_grid.c` | Coarse boolean grid for hidden face culling and ambient occlusion during voxel mesh build. Rasterizes boxes into cells, supports occupancy queries and per-vertex AO computation using the standard 3-neighbor corner sampling algorithm. |
| `lighting.h / lighting.c` | Simple lighting system. One directional light plus up to 8 point lights. Uploads lighting parameters as uniforms to the voxel shader. Half-Lambert diffuse for soft look, quadratic point light falloff. |

## Shaders

Three shader programs are compiled at init, embedded as string constants:

1. **UI shader** — Flat-colored quads. Uniforms: `u_projection` (mat4), `u_rect` (vec4: x,y,w,h), `u_color` (vec4).
2. **UI texture shader** — Textured quads for text glyphs. Same uniforms plus `u_texture` (sampler2D). Samples the red channel as alpha.
3. **Voxel shader** — Lit 3D geometry. Vertex inputs: position (vec3, loc 0), normal (vec3, loc 1), color (vec4, loc 2). Uniforms: `u_vp` (view-projection mat4), `u_model` (model mat4), lighting uniforms (directional + point lights). Fragment outputs half-Lambert diffuse + ambient + point light contributions.

## Future

- Post-processing (vignette, bloom) will be a separate pass.
- Shadow maps for directional light.
