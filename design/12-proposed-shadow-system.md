# 12 -- Proposed Shadow System (Multi-Plane Blob Shadows)

**Status:** Proposed — implement if current simple fix proves insufficient.

## Problem Statement

The current shadow system renders one shadow quad per entity at a single Y height,
chosen based on the entity's current tile. This causes artifacts:

- **Transition flicker:** When an actor moves from a normal tile (Y=0) to a conveyor
  tile (Y=0.078), the shadow is invisible on the conveyor during animation because
  it's rendered at floor height (behind the belt surface). The shadow pops in when
  the actor lands.
- **Depth test failures:** In the isometric view at 65° pitch, wall faces project
  downward over floor/conveyor areas. Their depth values can be closer to the camera
  than a shadow quad at surface level, causing GL_LESS to reject the shadow.
- **Single-plane limitation:** Each shadow draw picks one plane. Entities near
  elevation transitions (conveyor edges, groove rims) show abrupt shadow
  appear/disappear instead of smooth coverage.

## Current Simple Fix

Draw each actor's shadow on **every relevant plane** simultaneously (floor, conveyor,
trench). The depth test naturally shows only the visible one. During transitions,
shadows exist on both planes, eliminating flicker. This is a minimal code change.

If this fix proves sufficient, the system below is not needed.

## Proposed Architecture

### Shadow-Receiving Planes

There are four distinct flat-surface heights where shadows can appear:

| Plane | Y Height | Geometry |
|-------|----------|----------|
| **Floor** | 0.0 | Normal walkable tiles |
| **Trench** | -trench_depth | Groove box trench floor |
| **Feature** | ~0.078 | Conveyor belt top, potentially other elevated features |
| **Wall top** | WALL_HEIGHT (0.45) | Top faces of walls |

The plane list is parameterized as an array of heights, not hardcoded, so new
surface heights can be added without architectural changes.

### Per-Plane Shadow Textures

For each shadow plane, maintain one shadow texture (render target) covering the
visible playfield:

1. **Clear** the shadow texture to white (no shadow).
2. **Render shadow blobs** into the texture for all entities that cast shadows onto
   that plane. Each blob is an orthographic projection of the entity's shadow
   texture from the light direction onto the plane.
3. **Blur** the entire texture with a separable Gaussian pass. This replaces
   per-entity blur configuration — all shadows share one blur radius for the
   standard directional light.
4. **Sample** the shadow texture when rendering the plane's surface geometry.
   The fragment shader darkens pixels based on the shadow texture value.

### Shadow-Casting Entities

Each shadow-casting entity declares which planes it affects:

| Entity | Floor | Trench | Feature | Wall Top |
|--------|-------|--------|---------|----------|
| Actors (Theseus, Minotaur) | yes | yes | yes | no |
| Groove boxes | yes | yes | no | no |
| Walls | yes | yes | yes | no |
| Diorama decorations | height-based check | — | — | — |

### Rendering Pipeline

```
for each shadow plane (by height):
    1. bind plane's shadow FBO
    2. clear to white
    3. for each shadow caster affecting this plane:
         - render shadow blob (orthographic from light dir)
    4. two-pass separable Gaussian blur
    5. unbind FBO

main scene render:
    - when drawing floor/conveyor/trench/wall-top geometry:
      sample the corresponding plane's shadow texture
      darken fragment color accordingly
```

### Shadow Texture Mapping

Each shadow texture covers the grid extents. UV mapping from world position:

```
uv = (world_pos.xz - grid_origin) / grid_size
shadow = texture(u_shadow_plane_N, uv).r
```

### Design Decisions

- **Blob shadows, not shadow mapping.** True shadow mapping (rendering from the
  light's perspective to get silhouettes) would give accurate shadow shapes but
  conflicts with the cel-shaded art style. Blob shadows are an intentional
  aesthetic choice.
- **Unified blur radius.** One Gaussian blur pass per plane, same radius for all
  entities. Simpler than per-entity blur and visually consistent. Contact-hardening
  (sharper shadows for objects closer to surface) is sacrificed but not needed for
  this art style.
- **Texture, not stencil.** Shadow textures store continuous values (soft shadows)
  rather than binary stencil bits. This avoids needing a separate blur-to-texture
  step and makes debugging easier (shadow textures can be visualized directly).
- **Additive accumulation.** Multiple shadow casters darken the same texture
  additively (`min` blending or multiply), so overlapping shadows combine
  naturally without double-darkening artifacts.

### Performance

- 3--4 planes x (1 clear + ~10 blob draws + 2 blur passes) = ~50 draw calls.
- Shadow textures can be low resolution (256x256 or 512x512 for the full grid)
  since blur softens them anyway.
- Blur is separable Gaussian — two fullscreen quad draws per plane.
- Total cost is modest for modern GPUs and well within budget for this game's
  visual complexity.

### Migration Path

1. Implement shadow FBO management (create/resize per plane).
2. Move shadow blob rendering from inline draw calls to the per-plane pass.
3. Add blur passes.
4. Modify surface fragment shaders to sample shadow textures instead of using
   dedicated shadow quads with alpha blending.
5. Remove the old `draw_shadow_at_y` / `draw_shadow` functions.
6. Remove shadow-specific depth test hacks (GL_GREATER, polygon offset).
