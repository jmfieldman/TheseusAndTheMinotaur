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
| `voxel_mesh.h / voxel_mesh.c` | Freeform box mesh builder. Accumulates axis-aligned boxes at arbitrary positions and dimensions, then builds a single VBO with hidden face culling and three-tier AO. Vertex layout: position (vec3) + normal (vec3) + color (vec4) + uv (vec2) + ao_mode (float) = 13 floats. Each box is tagged with an AoMode (NONE/ATLAS/LIGHTMAP) controlling per-face AO routing. Boxes can be flagged `no_cull` to skip face culling. |
| `occupancy_grid.h / occupancy_grid.c` | Coarse boolean grid for hidden face culling and ambient occlusion during voxel mesh build. Rasterizes boxes into cells, supports occupancy queries and per-vertex AO computation. |
| `ao_baker.h / ao_baker.c` | Raytraced ambient occlusion baker. For each texel on a face, casts 24 hemisphere rays against the occupancy grid. Produces 32×32 uint8 AO tiles (255 = fully lit, 0 = fully occluded). Uses Fibonacci hemisphere sampling and fixed-step ray marching. Used only for complex geometry (AO_MODE_ATLAS). |
| `floor_lightmap.h / floor_lightmap.c` | Floor shadow lightmap generator. Projects wall/obstacle footprints top-down onto a 2D R8 texture, applies Gaussian blur for soft edges, and adds per-tile surface effects (edge darkening + grain). Configurable per biome via FloorShadowConfig (shadow scale, offset, blur radius, intensity, resolution). |
| `diorama_gen.h / diorama_gen.c` | Procedural diorama generator. Transforms a Grid + BiomeConfig into a rich 3D voxel scene: floor tiles (checkerboard), stacked-block walls with corner blocks, door frames, impassable cells, feature markers, floor/wall decorations, lantern pillars with point lights, exit light, and edge border. Uses seeded xorshift32 RNG for deterministic decoration placement. Returns point lights via DioramaGenResult for lighting integration. |
| `lighting.h / lighting.c` | Simple lighting system. One directional light plus up to 8 point lights. Uploads lighting parameters as uniforms to the voxel shader. Half-Lambert diffuse for soft look, quadratic point light falloff. |

## Shaders

Three shader programs are compiled at init, embedded as string constants:

1. **UI shader** — Flat-colored quads. Uniforms: `u_projection` (mat4), `u_rect` (vec4: x,y,w,h), `u_color` (vec4).
2. **UI texture shader** — Textured quads for text glyphs. Same uniforms plus `u_texture` (sampler2D). Samples the red channel as alpha.
3. **Voxel shader** — Lit 3D geometry. Vertex inputs: position (vec3, loc 0), normal (vec3, loc 1), color (vec4, loc 2), uv (vec2, loc 3). Uniforms: `u_vp` (view-projection mat4), `u_model` (model mat4), lighting uniforms, `u_ao_texture` (sampler2D), `u_has_ao` (int). Fragment samples AO texture when `u_has_ao != 0`, multiplies into base color, then applies half-Lambert diffuse + ambient + point light contributions.

## AO Pipeline (Three-Tier Hybrid)

AO uses three techniques matched to geometry predictability:

### Tier 1: Floor Shadow Lightmap (AO_MODE_LIGHTMAP)
1. **Footprint projection** — Wall/obstacle XZ footprints are projected onto a 2D buffer, scaled and offset per biome config
2. **Gaussian blur** — Separable blur softens shadow edges (configurable radius)
3. **Surface effects** — Per-tile edge darkening (12%) + grain noise (6%)
4. **Texture upload** — R8 lightmap uploaded to GPU, sampled on texture unit 1

### Tier 2: Wall Vertex Heuristics (AO_MODE_NONE)
- Floor-seam gradient: darkest at y=0, fading by 30% of wall height (smoothstep)
- Top lip darkening: slight darkening near wall top
- Baked directly into vertex colors — no texture sampling needed

### Tier 3: Raytraced AO Atlas (AO_MODE_ATLAS)
1. **Occupancy grid** — All boxes rasterized into a coarse boolean grid
2. **Face visibility** — Hidden face culling via occupancy queries
3. **Atlas packing** — Visible AO_MODE_ATLAS faces packed into texture atlas (32×32 texels/face)
4. **Raycasting** — 24 hemisphere rays per texel, 12 max steps through occupancy grid
5. **Surface effects** — Edge darkening + grain noise post-bake
6. **Texture upload** — AO atlas (R8) on texture unit 0

The fragment shader branches on a per-vertex `ao_mode` float to select the correct AO source.

## Future

- Post-processing (vignette, bloom) will be a separate pass.
- Shadow maps for directional light.
