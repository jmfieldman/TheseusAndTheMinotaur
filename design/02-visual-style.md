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

### 2.2 Tile Proportions and Sizing

The logical game grid maps to world-space tiles. All actor and wall sizing is
defined **relative to tile size** so proportions stay consistent regardless of
grid dimensions or screen resolution.

| Element         | Size (relative to tile)      | Notes                               |
| --------------- | ---------------------------- | ----------------------------------- |
| Theseus         | ~40--50% of tile width/depth | Centered on tile                    |
| Minotaur        | ~75% of tile width/depth     | Centered on tile; noticeably larger |
| Wall inset      | ~10% of tile width per side  | Walls render in this border zone    |
| Playable center | ~80% of tile width/depth     | Area remaining after wall insets    |

- The **size difference** between Theseus and the Minotaur is an important
  design element -- it reinforces that the Minotaur is a physical threat and
  Theseus must rely on wit, not strength.
- Wall insets (~10% per side) ensure that even the larger Minotaur fits
  snugly inside tiles bounded by walls on all sides.

### 2.3 Mesh Properties

- **Low polygon counts** throughout.
- **Smooth bevels** on edges -- no hard 90-degree corners on voxel geometry.
- **Rounded primitives** for actors and interactive elements.
- Geometry complexity budget (per-level diorama): **TBD** (will depend on
  platform performance targets).

### 2.4 Actor Geometry

#### Theseus

- A **cube** (with beveled edges consistent with the voxel aesthetic).
- Warm gold/amber coloring (see §5.1).
- Has a consistent **"down" face** -- Theseus always appears right-side-up
  regardless of movement.

#### Minotaur

- A larger cube/block form (~75% tile size), dark red/brown coloring.
- **White voxel horns** protrude from the top of the Minotaur. The horns are
  a key visual identifier and always appear on the uppermost face when the
  Minotaur is at rest.
- Horn behavior during movement is described in §7.2.
- **Face:** The Minotaur has a **face** composed of smaller detail voxels
  (eyes, snout, brow) on the side facing the camera. The face is always on
  the **camera-facing side** -- after each roll, the face re-materializes on
  whichever side now faces the camera. Like the horns, the face retracts into
  the body as the roll begins and re-emerges on the new camera-facing side
  when the Minotaur comes to rest. This ensures the player always sees the
  Minotaur's expression regardless of roll orientation.
- The face can convey subtle **expression shifts** (e.g. angrier eyes when
  close to Theseus, neutral when far away) using small voxel rearrangements.

## 3. Materials

- **Mostly solid colors** with flat/matte shading (no specular highlights or
  metallic surfaces).
- **Very simple textures** where used -- subtle noise or gradient overlays to
  break up flat surfaces, not detailed texture maps.
- No transparency/translucency in standard geometry (simplifies rendering).

> **Open question:** Should any environmental features (e.g. magical effects,
> portals) use additive blending or glow effects as exceptions to the matte
> rule?

## 4. Lighting and Shadows

### 4.1 Base Lighting

- **Soft baked lighting** -- the primary illumination is pre-computed per
  diorama, not dynamic.
- **Ambient occlusion** in crevices and where voxels meet to add depth.
- **Gentle gradients** across surfaces (e.g. slight darkening toward the bottom
  of walls, subtle sky-light from above).
- Possible subtle **rim lighting** on actors to help them read against the
  diorama.

### 4.2 Shadow Casting

All voxel geometry and actors should **cast shadows**:

- **Diorama geometry** (walls, floor edges, decorative elements) casts shadows
  from the primary scene light direction. These may be baked into the diorama
  mesh for static elements.
- **Actors** (Theseus and Minotaur) cast **real-time shadows** onto the floor
  and nearby walls. Because actors move, their shadows must be dynamic.
- Shadow style: soft-edged (no harsh shadow boundaries), consistent with the
  matte diorama aesthetic.

### 4.3 Dynamic Lighting

Certain environmental elements emit **dynamic light** that affects nearby
geometry:

- **Lantern posts, torches, lava cracks, luminescent crystals** -- these emit
  localized light that illuminates and casts dynamic shadows from surrounding
  walls and actors.
- **Theseus** may carry a subtle warm glow (gold/amber) for readability.
- **Minotaur** may carry a subtle threatening tint (deep red) for readability.
- Dynamic lights should be **point lights or spot lights** with limited radius
  to keep the effect localized and performant.
- Walls and actors near dynamic light sources should cast **real-time shadows**
  from those sources, creating atmospheric interplay (e.g. the Minotaur's
  shadow looming on a wall lit by a nearby lantern).

### 4.4 Exit Tile Lighting

The exit tile has a distinctive **"god-light" effect** -- a subtle volumetric
cone of light shining down onto the tile from above. This draws the player's
eye and reinforces the exit as a goal.

- The god-light should be soft and warm, not blindingly bright.
- Combined with the exit tile's finish-flag border (see §5.2), this creates
  a clear and inviting visual target.

## 5. Color Palette

- **Muted palette** -- desaturated, earthy, slightly warm tones.
- **Limited contrast** -- no pure black or pure white; values stay in the
  mid-range.
- Each biome defines its own sub-palette (see [03 -- Level Design](03-level-design.md)),
  but all biomes share the overall muted/low-contrast philosophy.

### 5.1 Functional Color Rules

Certain gameplay-critical elements need consistent color language across all
biomes:

| Element     | Color Direction                                             |
| ----------- | ----------------------------------------------------------- |
| Theseus     | Warm, identifiable (gold/amber family)                      |
| Minotaur    | Dark, threatening (deep red/brown)                          |
| Exit tile   | Distinct & inviting (soft green/teal), god-light from above |
| Walls       | Derived from biome palette                                  |
| Floor tiles | Derived from biome palette, subtle grid distinction         |
| Hazards     | Warning tones (muted orange/red)                            |

Environmental features and hazards should be visually apparent from their
styling alone -- no explicit highlight or outline system. The combination of
distinct geometry, functional color rules (above), and biome-specific palettes
should provide sufficient readability without an overlay system.

### 5.2 Exit Tile Presentation

The exit tile has two distinctive visual treatments:

- **God-light:** A soft volumetric light cone shining down from above (see
  §4.4).
- **Finish-flag border:** The edges of the exit tile feature a subtle
  **black-and-white checkered/flag pattern**, evoking a finish line. This
  pattern should be understated (not garish) and complement the biome palette.

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

On iOS/iPadOS (portrait only), the game engine renders into a **square
viewport** at the top of the screen (see [06 -- Input](06-input.md) §5).
The camera framing must account for this square aspect ratio rather than
the full screen dimensions.

## 7. Animation

### 7.1 Theseus Movement: Hop

Theseus **hops** from tile to tile:

- The cube lifts off the current tile, arcs through the air, and lands on the
  destination tile.
- Theseus always maintains a consistent **"down" face** -- the cube does not
  rotate during the hop. It simply translates along a parabolic arc.
- The hop should feel **quick and light** -- responsive to player input.
- A subtle **squash on landing** (brief vertical compression) sells the impact
  without being cartoonish.
- Use subtle 'lean' so that it looks like Theseus is **leaning into the jump**
  at first, then **landing feet first**.

### 7.2 Minotaur Movement: Roll

The Minotaur **rolls** from tile to tile, creating a heavier, more menacing
feel:

- When moving one tile in any direction, the Minotaur **rotates 90 degrees**
  around its leading bottom edge, rolling onto the next tile. It also arcs
  slightly upward (a "jump-roll") so the motion reads as powerful, not
  sliding.
- The Minotaur does **not** have a fixed "down" face -- each roll changes
  which face is on top.
- **Horn and face behavior:**
  - The Minotaur has **white voxel horns** (top) and a **detail-voxel face**
    (camera-facing side) that are key visual identifiers.
  - As the Minotaur begins a roll, both the horns and face **retract** into
    the body (smooth, quick tween inward).
  - When the Minotaur lands on the destination tile and comes to rest:
    - The horns **extend back out from the new top face** (whichever face is
      now pointing upward).
    - The face **re-materializes on the new camera-facing side**.
  - This means the horns are **always on top** and the face is **always
    toward the camera** when the Minotaur is at rest, regardless of how many
    rolls have occurred.
- **Ground shake:** When the Minotaur lands, a subtle **localized screen shake
  / ground impact effect** plays around the landing tile. This can be
  implemented as:
  - A brief camera micro-shake (small, fast dampened oscillation).
  - A visual impact ring or dust puff at the landing position.
  - The effect should be **localized** -- felt more strongly near the
    Minotaur's position and fading with distance.
- Each of the Minotaur's two steps per turn should be visually distinct, with
  a **brief pause** between step 1 and step 2.

### 7.3 Environmental Features

- Subtle idle animations (e.g. spike traps vibrating before activation, gears
  turning, lava bubbling).
- State-change animations should be clear and predictable (see
  [03 -- Level Design](03-level-design.md) §3.1).

### 7.4 Transitions

- **Level enter:** Diorama assembles (voxels fall/slide into place, or camera
  zooms into the level node on the overworld).
- **Level exit:** Diorama disassembles or camera pulls back.

### 7.5 Non-Blocking Animation

Animations are **non-blocking** -- the player can input their next move while
the current turn's animations are still playing (see
[01 -- Core Mechanics](01-core-mechanics.md) §10). When this happens:

- All in-progress animations **fast-forward** to their final positions.
- The new turn's animations begin immediately.
- This means animation durations should be short enough to feel snappy by
  default, but long enough to be readable when the player is watching.
- When fast-forwarding Minotaur animations, horn/face retract/extend and
  ground shake effects are **skipped** -- the Minotaur snaps to position with
  horns on top and face toward the camera.

## 8. Post-Processing

Kept minimal to maintain the clean matte look:

- Subtle **vignette** (darken screen edges).
- Optional light **bloom** on specific emissive elements (exit tile god-light,
  lava glow, lantern light).
- No film grain, chromatic aberration, or heavy color grading.
