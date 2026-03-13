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

- The scene is composed of **axis-aligned boxes** ("voxels") that create a
  chunky, handcrafted diorama aesthetic. However, these boxes are **completely
  freeform** in placement and sizing -- they are **not snapped to any voxel
  grid**. Each box has an arbitrary position (any float coordinate) and
  arbitrary dimensions (any width, height, depth).
- This means:
  - A wall is composed of ~5--15 irregularly sized stone blocks with slight
    position jitter and mortar gaps between them.
  - A grass decoration is a thin sliver (e.g. 0.05 tall) sitting just above
    the floor surface.
  - A pebble is a tiny wide-flat box at an arbitrary floor position.
  - Wall blocks can have subtle random offsets to look rough and hand-stacked
    rather than perfectly aligned.
- The logical game grid (NxM tiles) is a separate concept from the visual voxel
  geometry. The renderer maps grid positions to world-space diorama coordinates.
  The procedural generator places boxes relative to tile positions but is free
  to add jitter, variation, and overlaps.
- All level dioramas are **procedurally generated** from logical level data +
  biome configuration. No per-level meshes are hand-authored. See
  [09 -- Content Pipeline](09-content-pipeline.md) §3 for the generation
  pipeline.

### 2.1.1 Occupancy Grid (Internal)

Although boxes are placed at freeform coordinates, the mesh builder maintains
a **coarse occupancy grid** (~8 subdivisions per game tile) as an internal
acceleration structure. This grid is used for two purposes only:

1. **Face culling:** Before emitting vertices for a box face, the builder
   checks whether the occupancy grid shows solid material on the other side.
   If fully occluded (e.g. the underside of a floor slab, a wall face
   pressed against another wall), the face is skipped. This eliminates
   ~40--60% of hidden geometry.
2. **Ambient occlusion:** For each emitted vertex, the builder samples the
   3 neighboring occupancy cells at that corner to compute a darkening
   multiplier (see §4.1). The coarse resolution produces smooth, natural AO
   at seams and corners.

The occupancy grid is **not a placement constraint** -- boxes are placed
freely, then rasterized into the grid for these two queries. The grid is
discarded after the mesh is built (zero runtime cost).

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
- Blue coloring — RGB(80, 168, 251) (see §5.1).
- Has a consistent **"down" face** -- Theseus always appears right-side-up
  regardless of movement.

#### Minotaur

- A larger cube/block form (~75% tile size), red coloring — RGB(239, 34, 34).
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
- **Baked ambient occlusion** -- computed offline during the voxel mesh
  generation phase (not per-frame). After all boxes are placed, the mesh
  builder rasterizes them into the coarse occupancy grid (see §2.1.1).
  For each emitted vertex, it samples the 3 neighboring occupancy cells at
  that corner to compute a 0.0--1.0 occlusion multiplier, which is baked
  directly into the **vertex color** as a darkening factor. This adds depth
  at voxel junctions, wall-floor seams, inside corners, and beneath
  overhangs without any runtime cost. The AO calculation runs once when the
  diorama mesh is built and the shading is permanently encoded in the vertex
  data.
- **Gentle gradients** across surfaces (e.g. slight darkening toward the bottom
  of walls, subtle sky-light from above). These can also be baked into vertex
  colors during generation.
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
- **Theseus** may carry a subtle cool glow (blue) for readability.
- **Minotaur** may carry a subtle threatening tint (red) for readability.
- Dynamic lights should be **point lights or spot lights** with limited radius
  to keep the effect localized and performant.
- Walls and actors near dynamic light sources should cast **real-time shadows**
  from those sources, creating atmospheric interplay (e.g. the Minotaur's
  shadow looming on a wall lit by a nearby lantern).

### 4.3a Light Temperature Design

The scene uses a deliberate **cool/warm light temperature contrast** to guide
the player's eye and create atmospheric depth:

- **Cool lights** (cyan/blue): Lantern pillars, ambient fill, Theseus glow.
  These set the moody, atmospheric baseline and highlight the player character.
- **Warm lights** (gold/amber): Exit tile god-light. These draw attention to
  the goal.
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
| Theseus     | Cool blue — RGB(80, 168, 251)                               |
| Minotaur    | Threatening red — RGB(239, 34, 34)                          |
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

### 6.3 Projection Mode — Open Question

> **Status:** Under evaluation. Toggle at runtime with 'C' key; FOV adjustable in Settings.

The default projection is **orthographic**, which preserves grid readability and
ensures tiles appear the same size everywhere on screen. However, the zoom
transition between the overworld map and the puzzle diorama may look unnatural
with orthographic projection — objects scale uniformly with no depth cues.

A **perspective projection** with a very narrow FOV (e.g. 20°) looks nearly
identical to orthographic during normal gameplay, but produces a more natural
zoom-in/zoom-out feel when the camera distance changes. The trade-off is subtle
perspective distortion at screen edges that increases with wider FOV.

**Runtime toggle:** Press 'C' to switch between orthographic and perspective
projection. The vertical FOV is configurable in Settings (5°–90°, default 20°).
Both settings persist to `settings.yml` (`camera_perspective`, `camera_fov`).

**Decision criteria:**
- If zoom transitions look acceptable with ortho, keep ortho for simplicity
- If perspective zoom is clearly better, switch default to perspective with a
  narrow FOV (~15–25°) that minimizes edge distortion

### 6.4 iOS Layout Constraint

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

#### 7.3.1 Per-Feature Animation Details

Each environmental feature records typed animation events during turn resolution
(see [08 -- Engine Architecture](08-engine-architecture.md) §3.4.2). The
animation queue plays these back with feature-specific visuals:

**Theseus move variants:**

| Feature | Animation | Timing |
|---------|-----------|--------|
| Normal move | Parabolic hop from A to B | 0.15s |
| Ice slide | Hop to first ice tile, then constant-velocity linear slide through remaining tiles (no hop during slide) | 0.15s + 0.06s/tile |
| Teleporter | Scale down / fade out at source, then scale up / fade in at destination | 0.10s + 0.10s |
| Groove box push | Box slides one tile (linear) while Theseus steps into vacated tile — both animate concurrently | 0.15s |
| Manual turnstile | Walls rotate 90° smoothly around junction pivot; Theseus slides to destination tile concurrently | 0.20s |

**On-leave effects** (play after Theseus move completes):

| Feature | Animation | Timing |
|---------|-----------|--------|
| Crumbling floor | Tile darkens and visually collapses into a pit | 0.15s |
| Locking gate | Wall/bars appear with a slam effect (scale from 0→1) | 0.12s |
| Pressure plate | Target walls appear/disappear with brief flash | 0.10s |

**Environment phase** (play sequentially, replacing any static pause):

| Feature | Animation | Timing |
|---------|-----------|--------|
| Spike trap | Spikes extend upward (armed→active) or retract (active→inactive) with color interpolation | 0.12s |
| Auto-turnstile | Walls rotate 90° smoothly; actors on junction tiles slide to new positions concurrently | 0.25s |
| Moving platform | Platform tile slides from old to new path position; riding actors move together | 0.20s |
| Conveyor | Actor slides one tile in conveyor direction (linear, no hop) | 0.15s |

If no environment events exist, a minimal 0.10s pause maintains turn rhythm.

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

### 7.5 Input Buffering During Animation

Animations always play out **fully** -- they are never fast-forwarded or
skipped. See [01 -- Core Mechanics](01-core-mechanics.md) §10 for the
complete input buffering specification.

The **buffer window** is open during any animation phase (forward or reverse).
The player may press a key at any time while animations are playing to buffer
their next action. When the animation completes, the buffered action fires
immediately, creating fluid turn chaining. When a buffered input is pending,
remaining animations play at a user-configurable speed multiplier (default 2×,
range 1×–4×, set via Settings → "Anim Speed") to reduce wait time while still
showing every animation frame.

### 7.6 Undo Rewind Animation

When the player presses Undo, the previous turn's animation plays in
**reverse** at **2× speed** with a "VHS rewind" visual overlay:

- **Phase order reversed:** Minotaur step 2 → step 1 → environment (events
  in reverse order) → on-leave effects (reversed) → Theseus move (reversed).
- **Visual overlay:** Semi-transparent blue tint with horizontal scan lines
  over the entire viewport during reverse playback.
- **Grid restore deferred:** The actual board state is not restored until the
  reverse animation completes, so the player sees the turn visually "unwind."
- **Speed:** All animation durations divided by 2.0× for snappy but readable
  rewind.
- **Fallback:** If no animation record exists (e.g. first load), undo snaps
  the board state instantly.

### 7.7 Death Animations

Each death cause has a distinct voxel animation. All death animations work
with Theseus's voxel body -- the individual boxes that compose him separate,
transform, or react physically to the cause of death.

All death animations are **reversible** -- when the player triggers **Undo**
from the death state, the animation plays in reverse (e.g. scattered voxels
reconstitute back into Theseus's body, a petrified Theseus un-freezes and
regains color). This creates a satisfying "rewind" feel. See
[01 -- Core Mechanics](01-core-mechanics.md) §10.5.

#### 7.7.1 Minotaur Squish (Minotaur moves onto Theseus)

The Minotaur rolls onto Theseus's tile and **crushes** him:

- The Minotaur's roll animation plays normally onto the tile.
- On impact, Theseus's voxels **flatten and scatter outward** from the
  Minotaur's landing position -- as if squashed under the rolling cube.
- Voxels spray to the sides with a brief upward arc, then settle on the
  floor around the tile.
- Ground shake is stronger than usual (heavier impact).
- A subtle dust/debris puff accompanies the scatter.

**Undo reversal:** Scattered voxels slide back toward the tile center, lift
up, and reassemble into Theseus. The Minotaur rolls backward to its
previous position.

#### 7.7.2 Walk Into Minotaur (Theseus moves onto Minotaur)

Theseus hops toward the Minotaur and is **knocked back**:

- Theseus begins his hop animation toward the Minotaur's tile.
- Mid-arc, Theseus **collides** with the Minotaur -- the hop cuts short.
- Theseus's voxels **shatter on impact** and scatter backward (away from
  the Minotaur), as if he bounced off and broke apart.
- The Minotaur does a brief **recoil/flex** reaction (slight scale pulse).

**Undo reversal:** Voxels fly back from behind, reassemble into Theseus
mid-air, and he hops backward to his original tile.

#### 7.7.3 Spike Impale (Theseus on active spike trap)

Spikes shoot up through Theseus:

- The spike trap activates during the environment phase with Theseus on it.
- Spike geometry **extends upward through the tile** with force.
- Theseus's voxels **launch upward and outward** -- popped into the air by
  the spikes, then scatter and tumble down around the tile.
- The spikes remain extended with a few of Theseus's blue voxels impaled
  on them.

**Undo reversal:** Voxels fall back onto the spikes, slide down, reassemble
into Theseus. Spikes retract back into the floor.

#### 7.7.4 Medusa Petrification (Theseus faces Medusa)

Theseus turns to stone:

- As Theseus begins his move toward the Medusa, a **wave of grey** sweeps
  across his voxels from the Medusa's direction -- blue voxels shift to
  cold grey/stone color.
- Theseus **freezes mid-motion** (partial lean into the move direction).
- A brief pause while fully petrified, then the stone form **cracks and
  crumbles** -- voxels split along fracture lines and collapse downward
  into a small rubble pile on the tile.
- Faint dust particles rise from the rubble.

**Undo reversal:** Rubble voxels lift and reassemble, grey color sweeps back
to blue from the direction away from Medusa, Theseus unfreezes and steps
back.

#### 7.7.5 Pit Fall (Crumbling floor, missing platform, bottomless pit)

Theseus falls into the void:

- The floor beneath Theseus **crumbles away** (for crumbling floor) or is
  simply absent (missing platform / pit tile).
- Theseus's voxels **drop downward** into the hole -- not a scatter, but a
  coherent fall with slight tumble rotation.
- As Theseus falls, he **shrinks toward a vanishing point** below the
  diorama platform (disappearing into the depth).
- For lava/hazard pits: voxels glow orange/red as they descend and
  dissolve, with rising heat-shimmer particles.
- The pit opening remains visible (dark void or lava glow).

**Undo reversal:** Theseus rises back up out of the pit (voxels growing
from the vanishing point), the floor reassembles beneath him, and he steps
back to his previous tile.

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
