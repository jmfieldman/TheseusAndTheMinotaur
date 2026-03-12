# 02 -- Visual Style

## 1. Overview

The game is presented as a **matte, low-poly stylized voxel diorama** rendered
in 3D with an **orthographic projection**. The camera looks down at the puzzle
board at a fixed angle, preserving the 2D grid readability while giving depth
and atmosphere through 3D geometry and lighting.

The overall feel is that of a **miniature tabletop diorama** -- the playable
grid sits on a **raised platform** that is visually elevated above its
surroundings. The diorama edge drops away into a biome-themed border treatment
(cliff face, dense foliage, water, stone frame, etc.), reinforcing the sense
that the player is looking down at a self-contained miniature world. The
**back wall** (the wall farthest from the camera) is an exception to the
low-profile wall rule -- it rises tall behind the playable area and serves as
a **thematic backdrop** with rich biome-specific detailing (e.g. towering
overgrown ruins, carved stone reliefs, mechanical gantries). The scene should
feel **intimate and enclosed**, with atmospheric darkness pressing in at the
edges (aided by the vignette post-processing pass).

## 2. Geometry

### 2.1 Voxel Philosophy

- Voxels are the primary building block but are **not constrained to a uniform
  grid**. They are positioned artistically to create beautiful diorama pieces.
- Voxels **do not need to be perfect cubes** -- they can vary in width, height,
  and depth to create visual interest.
- The logical game grid (NxM tiles) is a separate concept from the visual voxel
  geometry. The renderer maps grid positions to world-space diorama coordinates.
- All level dioramas are **procedurally generated** from logical level data +
  biome configuration. No per-level meshes are hand-authored. See
  [09 -- Content Pipeline](09-content-pipeline.md) §3 for the generation
  pipeline.

### 2.1a Grid Visibility

The game grid is made **inherently visible** through a **checkerboard pattern**
on the floor tiles. Every other tile uses an offset color from the biome's
floor palette (~5--15% lightness shift). This ensures the player can always
perceive tile boundaries even in open areas with no walls, without requiring
explicit grid lines or overlays.

Each floor tile is itself composed of **multiple smaller voxel blocks** (like
individual paving stones or flagstones) with subtle per-block color variation.
This block composition gives the floor texture and visual richness while the
overall checkerboard pattern at the tile level preserves grid readability.

### 2.1b Floor Surface Detail

Floors are **not perfectly flat**. Small biome-themed voxel details are
scattered on walkable tiles by the procedural generator (e.g. small clover
patches in the Dark Forest, loose pebbles in the Catacombs, bolt heads in the
Mechanical Halls). These sit at or below floor level and are purely cosmetic --
they must not obscure gameplay-critical information. See
[09 -- Content Pipeline](09-content-pipeline.md) §3.2 for the full catalog.

### 2.2 Tile Proportions and Sizing

The logical game grid maps to world-space tiles. All actor and wall sizing is
defined **relative to tile size** so proportions stay consistent regardless of
grid dimensions or screen resolution.

| Element         | Size (relative to tile)      | Notes                               |
| --------------- | ---------------------------- | ----------------------------------- |
| Theseus         | ~40--50% of tile width/depth | Centered on tile                    |
| Minotaur        | ~75% of tile width/depth     | Centered on tile; noticeably larger |
| Wall inset      | ~10% of tile width per side  | Walls render in this border zone    |
| Wall height     | ~25--35% of tile width       | Low-profile; must not occlude actors|
| Playable center | ~80% of tile width/depth     | Area remaining after wall insets    |

- The **size difference** between Theseus and the Minotaur is an important
  design element -- it reinforces that the Minotaur is a physical threat and
  Theseus must rely on wit, not strength.
- Wall insets (~10% per side) ensure that even the larger Minotaur fits
  snugly inside tiles bounded by walls on all sides.
- **Wall height is intentionally low.** Walls should be just tall enough to
  clearly read as impassable barriers on tile edges, but **must not occlude
  actors or tile contents** behind them. With the fixed orthographic camera
  angle, a wall on the near (camera-facing) edge of a tile must not block
  the player's view of Theseus or the Minotaur standing on that tile. Think
  of them as raised curbs or low ledges rather than towering barriers.
- **Wall construction is visibly blocky.** Walls are composed of individually
  discernible **stacked stone/brick voxel blocks** with visible mortar gaps
  and per-block color variation. They should not look like smooth extruded
  surfaces -- the block composition is part of the charm. The block style
  varies per biome (e.g. regular rectangular blocks for Stone Labyrinth,
  irregular rough-cut stones for Dark Forest, aligned bronze panels for
  Mechanical Halls).
- **Passage archways:** Where two adjacent tiles have no wall between them
  but walls exist on neighboring edges (creating a corridor), the wall ends
  may form subtle **archway shapes** -- the top edge of the wall curves or
  steps inward toward the gap. This frames passages and makes openings in
  walls visually distinct from simply missing wall segments.
- **Back wall exception:** The **back wall** -- the wall along the far edge
  of the diorama (farthest from the camera) -- is exempt from the low-profile
  height rule. It rises **tall behind the playable area** and serves as a
  **decorative, thematic backdrop** for the diorama. Because it sits at the
  back of the orthographic view, it never occludes actors or gameplay
  elements. The back wall is **mostly aesthetic**, but may contain the
  entrance or exit door (see [01 -- Core Mechanics](01-core-mechanics.md)
  §7.1). When the exit door is in the back wall, the wall geometry around
  the opening must be **transparent or cut away** so the player can see
  Theseus stepping through it. The back wall's design is rich with
  biome-specific detail (e.g. towering vine-covered ruins, carved
  mythological reliefs, massive gear assemblies) and helps frame the diorama
  as a self-contained miniature world.
- **Entrance and exit doors:** Two openings in the boundary walls serve as
  the level's entrance and exit (see [01 -- Core Mechanics](01-core-mechanics.md)
  §7). These are the only breaks in the otherwise solid boundary. The
  entrance has a biome-themed locking mechanism (stone slab, iron bars,
  vine growth) that seals visibly after Theseus enters. The exit has a
  **virtual exit tile** extending one tile outward from the grid, with
  god-light shining through the opening (see §4.4).

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
- **Glow exception:** Light-emitting elements (lantern crystals, lava surfaces,
  exit tile) may use **additive blending or emissive vertex colors** to create
  a soft glow effect. This is the one exception to the matte rule -- glow
  should be used sparingly and only on gameplay-relevant light sources.

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

- **Lantern pillars** are the primary dynamic light source on most biomes.
  These are **tall decorative columns** (roughly actor height, taller than
  walls) topped with a **glowing crystal or flame element**. They are placed
  at diorama edges, corners, or wall endpoints by the procedural generator.
  Lantern light uses a **cool color temperature** (cyan/blue/teal family) to
  contrast with the warm exit glow.
- **Other light sources** (torches, lava cracks, luminescent crystals) emit
  localized light with biome-appropriate color temperatures.
- **Theseus** may carry a subtle warm glow (gold/amber) for readability.
- **Minotaur** may carry a subtle threatening tint (deep red) for readability.
- Dynamic lights should be **point lights or spot lights** with limited radius
  to keep the effect localized and performant.
- Walls and actors near dynamic light sources should cast **real-time shadows**
  from those sources, creating atmospheric interplay (e.g. the Minotaur's
  shadow looming on a wall lit by a nearby lantern).

### 4.3a Light Temperature Design

The scene uses a deliberate **cool/warm light temperature contrast** to guide
the player's eye and create atmospheric depth:

- **Cool lights** (cyan/blue): Lantern pillars, ambient fill. These set the
  moody, atmospheric baseline of the scene.
- **Warm lights** (gold/amber): Exit tile god-light, Theseus glow. These draw
  attention to the goal and the player character.
- **Hot lights** (red/orange): Lava, fire hazards, Minotaur glow. These signal
  danger.

This temperature separation ensures that even at a glance, the player can
distinguish ambient atmosphere (cool) from objectives (warm) from threats
(hot).

### 4.4 Exit Door Lighting

The exit door has a distinctive **"god-light" effect** -- a warm volumetric
light that shines **through the exit door opening** into the diorama,
illuminating the floor tiles near the exit and casting light onto the boundary
wall edges around the opening. This draws the player's eye and reinforces the
exit as the goal.

- The god-light should be **warm golden/amber** in color -- soft and inviting,
  not blindingly bright. It should feel like sunlight or warmth pouring in
  from outside the labyrinth.
- The warm exit glow deliberately contrasts with the cool-toned lantern
  lighting that fills the rest of the scene (see §4.3a), making the exit
  instantly identifiable.
- The **virtual exit tile** (the floor tile just outside the door opening)
  is the brightest point, bathed in the warm glow.
- Combined with the exit door's finish-flag border (see §5.2), this creates
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
| Exit door   | Warm & inviting (golden/amber glow), god-light through opening |
| Walls       | Derived from biome palette                                  |
| Floor tiles | Derived from biome palette, subtle grid distinction         |
| Hazards     | Warning tones (muted orange/red)                            |

Environmental features and hazards should be visually apparent from their
styling alone -- no explicit highlight or outline system. The combination of
distinct geometry, functional color rules (above), and biome-specific palettes
should provide sufficient readability without an overlay system.

### 5.2 Exit Door Presentation

The exit door has two distinctive visual treatments:

- **God-light:** Warm volumetric light pouring through the door opening from
  outside (see §4.4).
- **Finish-flag border:** The floor edges around the exit door opening and
  the virtual exit tile feature a subtle **black-and-white checkered/flag
  pattern**, evoking a finish line. This pattern should be understated (not
  garish) and complement the biome palette.

## 6. Camera

- **Orthographic projection** to preserve grid readability.
- **Fixed camera angle** across all puzzle levels (no per-biome variation, no
  player-controlled rotation). This ensures consistent spatial reasoning for
  the player.
- Camera framing adjusts to fit the puzzle grid with appropriate padding for UI.

### 6.1 Camera Angle

The camera looks down at the diorama from a **configurable elevation angle**
defined as a single engine constant (e.g. `CAMERA_ELEVATION_DEG`). This
angle is measured from the horizontal plane:

- **0°** = looking straight at the side (fully horizontal)
- **90°** = looking straight down (top-down)
- **Target range: ~30--40°** (based on art direction mockups)

This constant is the **single source of truth** for camera orientation. All
downstream systems that depend on the camera angle must derive their values
from it rather than using independent hard-coded numbers:

- **Back wall height** (how tall before it exits the top of the frame)
- **Wall occlusion threshold** (max wall height before it blocks tile contents
  behind it at this angle)
- **Side door visibility** (how much of the virtual exit tile is visible
  through side wall openings)
- **Shadow projection angle** (baked shadow direction)
- **Orthographic bounds calculation** (how much floor vs. front-face is
  visible at a given angle)
- **Vignette center offset** (vignette should center on the diorama, which
  shifts vertically with camera angle)

The angle should be tuned visually during development. Changing it should
not require touching any other constants -- everything adapts automatically.

### 6.2 Camera Considerations by Grid Size

- Small grids (4x4): Diorama fills the screen comfortably.
- Large grids (16x16): May need to zoom out. Must ensure tiles remain large
  enough to read on all target screen sizes (especially mobile).

### 6.3 iOS Layout Constraint

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

- Environmental features should have **real 3D depth** -- they are not flat
  decals on the floor. A pressure plate is a visibly recessed slab sitting
  slightly below floor level; spike traps have visible machinery beneath the
  surface; a stairway descends into the diorama platform.
- Subtle idle animations (e.g. spike traps vibrating before activation, gears
  turning, lava bubbling, pressure plate ticking).
- State-change animations should be clear and predictable (see
  [03 -- Level Design](03-level-design.md) §3.1).

### 7.4 Transitions

- **Level enter (from overworld):** Camera **zooms in** from the overworld
  view to the selected puzzle node. The low-detail overworld mesh fades to
  the high-detail puzzle mesh during the zoom. On arrival, the level start
  sequence plays (see [01 -- Core Mechanics](01-core-mechanics.md) §7.5).
- **Auto-progression (puzzle → puzzle):** Camera **zooms out** from the
  current puzzle to a mid-level view showing both dioramas on the overworld,
  then **pans** across the overworld to the next puzzle node, then **zooms
  in** to the next puzzle. The LOD meshes swap during zoom transitions.
- **Level exit (return to overworld):** Camera **zooms out** from the puzzle
  back to the full overworld view.

### 7.6 Level Start and Reset Animation

The level start and reset sequences have distinct visual choreography:

**First entry (zoom in from overworld):**

1. Camera zoom completes, high-detail diorama is visible.
2. Entrance door is open; exit door is visible with god-light.
3. Minotaur **drops from above** onto his starting tile (ground shake).
4. Theseus **hops in** through the entrance door onto the first interior tile.
5. Entrance door **locks shut** with biome-themed animation (stone slab, bars,
   vines, etc.).

**Mid-level reset:**

1. Theseus and the Minotaur **lift upward** off the board and disappear
   (quick upward tween off-screen).
2. Environmental features **reset** to initial states (animated reversal).
3. Entrance door **unlocks and opens**.
4. Minotaur drops in from above (ground shake).
5. Theseus hops in through the entrance door.
6. Entrance door locks shut.

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

- **Vignette** (darken screen edges) -- should be **moderately pronounced**,
  not just a hint. The darkened edges create an intimate, enclosed atmosphere
  and focus the player's eye on the diorama center. The vignette should make
  the diorama feel like it's being spotlit from above in a dark room.
- **Bloom** on emissive elements (exit tile god-light, lantern pillar glow,
  lava glow). Bloom should be soft and contained -- it should make glowing
  elements feel luminous without washing out the scene.
- No film grain, chromatic aberration, or heavy color grading.
