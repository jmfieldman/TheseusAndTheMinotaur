# src/render/

OpenGL rendering subsystem. Handles shader management, 2D UI drawing, text rendering, and 3D voxel diorama rendering with raytraced ambient occlusion.

## Files

| File                | Purpose |
|---------------------|---------|
| `shader.h / shader.c` | Shader compilation and uniform utilities. `shader_compile()` takes vertex + fragment source strings, returns a linked GLuint program. Uniform setters for mat4, vec4, vec2, float, int. |
| `renderer.h / renderer.c` | Core renderer. Creates shared quad VBO (unit quad 0–1), compiles three built-in shaders (flat color UI, textured UI, voxel), and manages the projection matrix. Supports orthographic (default) and perspective projection modes, toggled via `g_settings.camera_perspective`. In perspective mode, builds a combined P×V matrix calibrated so that z=0 geometry renders pixel-identically to ortho. Provides `renderer_begin_frame()` / `renderer_end_frame()`. |
| `ui_draw.h / ui_draw.c` | Immediate-mode 2D drawing. `ui_draw_rect()` for filled rectangles, `ui_draw_rect_rounded()` for rounded corners (triangle fan per corner, 8 segments). All coordinates in screen-space pixels. |
| `text_render.h / text_render.c` | Text rendering via SDL_ttf. Loads a single TTF at 5 sizes (16, 24, 32, 48, 64). Renders text to GL textures with an LRU cache (128 entries). Supports LEFT/CENTER/RIGHT alignment. Uses a single-channel (GL_RED) texture with the alpha from SDL_ttf's blended output. |
| `camera.h / camera.c` | Isometric-style diorama camera. Orthographic projection with configurable yaw/pitch angles, target point, and view size. Produces a combined view-projection matrix for the voxel shader. |
| `voxel_mesh.h / voxel_mesh.c` | Freeform box mesh builder. Accumulates axis-aligned boxes at arbitrary positions and dimensions, then builds a single VBO with hidden face culling and raytraced AO texture. Vertex layout: position (vec3) + normal (vec3) + color (vec4) + uv (vec2) = 12 floats. Boxes can be flagged `no_cull` (for thin geometry like walls) to skip face culling while still contributing to the occupancy grid for AO. During build, each visible face gets an 8×8 texel tile in an AO texture atlas. |
| `occupancy_grid.h / occupancy_grid.c` | Coarse boolean grid for hidden face culling and ambient occlusion during voxel mesh build. Rasterizes boxes into cells, supports occupancy queries and per-vertex AO computation. |
| `ao_baker.h / ao_baker.c` | Raytraced ambient occlusion baker. For each texel on a face, casts 32 cosine-weighted hemisphere rays against the occupancy grid. Produces 8×8 uint8 AO tiles (255 = fully lit, 0 = fully occluded). Uses Fibonacci hemisphere sampling and fixed-step ray marching. |
| `diorama_gen.h / diorama_gen.c` | Procedural diorama generator. Transforms a Grid + BiomeConfig into a rich 3D voxel scene via a 12-step pipeline: platform, floor tiles (2×2 paving stones), stacked-block walls with mortar/jitter, back wall, door frames, impassable cells, feature markers, floor/wall decorations, lantern pillars with point lights, exit light, and edge border. Uses seeded xorshift32 RNG for deterministic decoration placement. Returns point lights via DioramaGenResult for lighting integration. |
| `lighting.h / lighting.c` | Simple lighting system. One directional light plus up to 8 point lights. Uploads lighting parameters as uniforms to the voxel shader. Half-Lambert diffuse for soft look, quadratic point light falloff. |

## Shaders

Three shader programs are compiled at init, embedded as string constants:

1. **UI shader** — Flat-colored quads. Uniforms: `u_projection` (mat4), `u_rect` (vec4: x,y,w,h), `u_color` (vec4).
2. **UI texture shader** — Textured quads for text glyphs. Same uniforms plus `u_texture` (sampler2D). Samples the red channel as alpha.
3. **Voxel shader** — Lit 3D geometry. Vertex inputs: position (vec3, loc 0), normal (vec3, loc 1), color (vec4, loc 2), uv (vec2, loc 3). Uniforms: `u_vp` (view-projection mat4), `u_model` (model mat4), lighting uniforms, `u_ao_texture` (sampler2D), `u_has_ao` (int). Fragment samples AO texture when `u_has_ao != 0`, multiplies into base color, then applies half-Lambert diffuse + ambient + point light contributions.

## AO Pipeline

The ambient occlusion pipeline uses precomputed textures rather than per-vertex baking:

1. **Occupancy grid** — All boxes (including walls) are rasterized into a coarse boolean grid
2. **Face visibility** — Each face is checked against the occupancy grid for culling (unless `no_cull`)
3. **Atlas packing** — Visible faces are packed into a texture atlas (8×8 texels per face)
4. **Raycasting** — For each texel, 32 hemisphere rays are marched through the occupancy grid
5. **Texture upload** — The AO atlas (R8 format) is uploaded to GPU and sampled in the fragment shader

This produces smooth, artifact-free AO shadows at wall-floor junctions without requiring geometry subdivision.

## Future

- Post-processing (vignette, bloom) will be a separate pass.
- Shadow maps for directional light.
