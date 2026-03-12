# 02 -- Visual Style

## 1. Overview

The game is presented as a **matte, low-poly stylized voxel diorama** rendered
in 3D with an **orthographic projection**. The camera looks down at the puzzle
board at a fixed angle, preserving the 2D grid readability while giving depth
and atmosphere through 3D geometry and lighting.

## 2. Geometry

### 2.1 Voxel Philosophy

- Voxels are the primary building block but are **not constrained to a uniform
  grid**. They are positioned artistically to create beautiful diorama pieces.
- Voxels **do not need to be perfect cubes** -- they can vary in width, height,
  and depth to create visual interest.
- The logical game grid (NxM tiles) is a separate concept from the visual voxel
  geometry. The renderer maps grid positions to world-space diorama coordinates.

### 2.2 Mesh Properties

- **Low polygon counts** throughout.
- **Smooth bevels** on edges -- no hard 90-degree corners on voxel geometry.
- **Rounded primitives** for actors and interactive elements.
- Geometry complexity budget (per-level diorama): **TBD** (will depend on
  platform performance targets).

## 3. Materials

- **Mostly solid colors** with flat/matte shading (no specular highlights or
  metallic surfaces).
- **Very simple textures** where used -- subtle noise or gradient overlays to
  break up flat surfaces, not detailed texture maps.
- No transparency/translucency in standard geometry (simplifies rendering).

> **Open question:** Should any environmental features (e.g. magical effects,
> portals) use additive blending or glow effects as exceptions to the matte
> rule?

## 4. Lighting

- **Soft baked lighting** -- the primary illumination is pre-computed per
  diorama, not dynamic.
- **Ambient occlusion** in crevices and where voxels meet to add depth.
- **Gentle gradients** across surfaces (e.g. slight darkening toward the bottom
  of walls, subtle sky-light from above).
- No harsh shadows or sharp shadow edges.
- Possible subtle **rim lighting** on actors to help them read against the
  diorama.

### 4.1 Dynamic Lighting (Limited)

- The Minotaur and Theseus may carry subtle light contributions (e.g. a warm
  glow around Theseus, a threatening red tint near the Minotaur) for gameplay
  readability.
- Environmental features with state changes may pulse or shift color
  temperature.
- These dynamic effects should be minimal and not break the baked-lighting
  aesthetic.

## 5. Color Palette

- **Muted palette** -- desaturated, earthy, slightly warm tones.
- **Limited contrast** -- no pure black or pure white; values stay in the
  mid-range.
- Each biome defines its own sub-palette (see [03 -- Level Design](03-level-design.md)),
  but all biomes share the overall muted/low-contrast philosophy.

### 5.1 Functional Color Rules

Certain gameplay-critical elements need consistent color language across all
biomes:

| Element          | Color Direction                        |
|------------------|----------------------------------------|
| Theseus          | Warm, identifiable (gold/amber family) |
| Minotaur         | Dark, threatening (deep red/brown)     |
| Exit tile        | Distinct & inviting (soft green/teal)  |
| Walls            | Derived from biome palette             |
| Floor tiles      | Derived from biome palette, subtle grid distinction |
| Hazards          | Warning tones (muted orange/red)       |

> **Open question:** Should there be a consistent highlight/outline system for
> interactive or dangerous tiles, or rely purely on color and geometry?

## 6. Camera

- **Orthographic projection** to preserve grid readability.
- **Fixed camera angle** across all puzzle levels (no per-biome variation, no
  player-controlled rotation). This ensures consistent spatial reasoning for
  the player.
- Camera framing adjusts to fit the puzzle grid with appropriate padding for UI.

### 6.1 Camera Considerations by Grid Size

- Small grids (4x4): Diorama fills the screen comfortably.
- Large grids (16x16): May need to zoom out. Must ensure tiles remain large
  enough to read on all target screen sizes (especially mobile).

### 6.2 iOS Layout Constraint

On iOS/iPadOS, the game engine renders into a **square viewport** (see
[06 -- Input](06-input.md) §5). The camera framing must account for this
square aspect ratio rather than the full screen dimensions.

## 7. Animation

- **Theseus movement:** Smooth slide/hop between tiles (quick, responsive).
- **Minotaur movement:** Slightly heavier/more menacing animation. Each of the
  two steps should be visually distinct (brief pause between step 1 and step 2).
- **Environmental features:** Subtle idle animations (e.g. spike traps
  vibrating before activation, gears turning).
- **Transitions:** Diorama assembles/disassembles for level enter/exit.

### 7.1 Non-Blocking Animation

Animations are **non-blocking** -- the player can input their next move while
the current turn's animations are still playing (see
[01 -- Core Mechanics](01-core-mechanics.md) §10). When this happens:

- All in-progress animations **fast-forward** to their final positions.
- The new turn's animations begin immediately.
- This means animation durations should be short enough to feel snappy by
  default, but long enough to be readable when the player is watching.

## 8. Post-Processing

Kept minimal to maintain the clean matte look:

- Subtle **vignette** (darken screen edges).
- Optional light **bloom** on specific emissive elements only.
- No film grain, chromatic aberration, or heavy color grading.
