# 11 — Implementation Plan

Ordered steps for completing the game, from current state to shippable. Each step is a discrete, testable unit of work. Steps build on each other — complete them in order.

## What's Done

- **Build system** — CMake + FetchContent (SDL3, SDL3_ttf, cJSON, libyaml, glad)
- **Engine core** — Fixed-timestep loop, stack-based state manager, State vtable
- **Input system** — Semantic action abstraction, keyboard + gamepad adapters
- **Rendering foundation** — Shader compilation, orthographic UI, flat-color + textured quads, text rendering (SDL_ttf → GL texture cache)
- **Data layer** — String table (JSON), settings (YAML), save data (YAML), platform paths
- **Menu scenes** — Title screen, save select, settings overlay
- **Game logic** — Grid/cell/wall model, turn resolution (3-phase), Minotaur AI (greedy 2-step), undo/redo, level loader (JSON), pluggable feature vtable, spike trap

---

## Step 1 — Puzzle Scene (Text-Mode Prototype)

Wire the game logic into a playable scene using only the existing 2D UI renderer. No 3D, no diorama — just colored rectangles for tiles, walls, Theseus, and Minotaur.

**Files:**

- `src/scene/puzzle_scene.h / .c` — Implements `State` vtable
- `assets/levels/tutorial/tutorial-01.json` — First test level (simple 4×4, walls only, no features)
- `assets/levels/tutorial/tutorial-02.json` — Second test level (introduces spike trap)

**Behaviour:**

- Load level JSON on enter
- Render grid as colored rectangles (checkerboard floor, walls as thick lines, Theseus as gold square, Minotaur as red square)
- Accept puzzle input context actions (MOVE\_\*, WAIT, UNDO, RESET, PAUSE)
- Resolve turns via `turn_resolve()`
- Display turn counter, win/loss text
- UNDO/RESET work correctly
- PAUSE pushes pause overlay (stub — reuse settings scene or minimal overlay)
- Win → display "Level Complete" + turn count vs. optimal
- Back returns to save select (or title for now)

**Verification:** Play through both tutorial levels entirely with keyboard. Undo mid-level, reset, win, lose to Minotaur, lose to spike trap.

---

## Step 2 — Remaining Environmental Features

Implement all feature types from the design doc (§03 §3.2). Each is a
self-contained `.c` file + registration in the feature registry.

**Files (one pair each):**

- `src/game/features/pressure_plate.h / .c` — Permanently toggles linked walls/tile passability when Theseus steps on it. Color-coded tints for visual association. Minotaur does not trigger.
- `src/game/features/locking_gate.h / .c` — One-way door that locks (bars come up) permanently after any actor passes through.
- `src/game/features/auto_turnstile.h / .c` — Automatic turnstile at junction of 4 tiles. Rotates 90° each environment phase, moving walls, actors, and features on those tiles.
- `src/game/features/manual_turnstile.h / .c` — Player-activated turnstile. Theseus pushes against a wall at the junction to rotate all connected walls 90°. Floor/actors don't move. Minotaur cannot trigger.
- `src/game/features/teleporter.h / .c` — Paired tiles; step on one → instantly appear at other. Both actors trigger. No chaining.
- `src/game/features/crumbling_floor.h / .c` — Passable once. Collapses into deadly pit during environment phase after Theseus leaves. If Theseus waits on it, it collapses and he dies. Minotaur immune to collapse.
- `src/game/features/moving_platform.h / .c` — Floating tile over bottomless pit. Moves one step along a defined path each environment phase (pingpong or loop). Actors ride the platform. Pit tiles without platform are deadly/impassable.
- `src/game/features/medusa_wall.h / .c` — Wall-mounted face with line-of-sight. If Theseus moves toward the Medusa while in its sightline, he dies (on_pre_move → PREMOVE_KILL). Walls block sightline. Minotaur immune.
- `src/game/features/ice_tile.h / .c` — Theseus slides in move direction until hitting a wall or non-ice tile (all in one Theseus phase). Minotaur moves normally on ice.
- `src/game/features/groove_box.h / .c` — Heavy box in a fixed groove track. Theseus can push it along the groove. Blocks both actors. Minotaur cannot push.

**Also:**

- Test levels in `assets/levels/test/` for each feature type
- Update `feature_registry.c` with all new factories
- Update `turn.c` to call `on_pre_move` and `on_push` hooks

**Verification:** One test level per feature type, playable in the text-mode puzzle scene. Verify undo restores feature state correctly for every feature.

---

## Step 3 — Animation System

Tween-based animation framework with event-driven per-feature playback and input buffering. Game logic resolves instantly and records typed animation events (`AnimEvent`) into the `TurnRecord`. The animation queue replays these events with per-feature visual effects. Animations always play out fully; when the player buffers input, remaining animations play at a user-configurable speed (default 2×, range 1×–4×, set in Settings). The input buffer window is open during any animation phase (forward or reverse). See [01 -- Core Mechanics](01-core-mechanics.md) §10 for the full input buffering specification.

**Files:**

- `src/engine/tween.h / .c` — Tween primitives: lerp position, lerp rotation, lerp color, easing functions (linear, ease-in-out, parabolic arc for hop, out-back, quad, cubic). Tween struct with start/end values, duration, elapsed, easing function pointer.
- `src/game/anim_event.h` — Animation event types. Defines `AnimEventType` enum (13 event types), `AnimEventPhase` enum (Theseus / Theseus-effect / Environment), and `AnimEvent` tagged union with type-specific data (positions, waypoints, directions, actor flags). Events are recorded during `turn_resolve()` and replayed by the animation queue. Max 32 events per turn.
- `src/engine/anim_queue.h / .c` — Turn animation sequencer. Plays the turn's animation in a 5-phase sequence: Theseus move → Theseus on-leave effects → Environment phase → Minotaur step 1 → Minotaur step 2. Scans `TurnRecord` events to determine animation type and dispatches to sub-phase logic:
  - **Normal hop:** Parabolic arc (0.15s)
  - **Ice slide:** Hop to first tile (0.15s) + constant-velocity slide through waypoints (0.06s/tile)
  - **Teleport:** Scale/fade out (0.10s) + scale/fade in (0.10s)
  - **Push:** Concurrent tweens for box slide + Theseus step (0.15s)
  - **Turnstile rotation:** Walls rotate 90° + Theseus slides (0.20s)
  - **On-leave effects:** Sequential playback of crumble (0.15s), gate lock (0.12s), plate toggle (0.10s)
  - **Environment events:** Sequential playback of spike change (0.12s), auto-turnstile (0.25s), platform move (0.20s), conveyor push (0.15s)
  - Provides query functions for renderers: teleport progress, aux position (box/platform), rotation progress, current event, ice-slide state.
- `src/engine/input_buffer.h / .c` — Single-slot input buffer. Buffer window is open during any animation phase (forward or reverse). Accepts fresh key presses (not held). Last press wins. Commit on animation complete. Distinguishes held keys (ignored) from fresh presses (buffered). When a buffered action is pending, remaining animations play at user-configurable speed (`g_settings.anim_speed`, default 2×, range 1×–4×).

**AnimEvent recording mechanism:**

- `Grid` holds a transient `active_record` pointer (set at start of `turn_resolve()`, cleared at end)
- Features push events from existing vtable hooks via `turn_record_push_event()` — no vtable signature changes needed
- `turn.c` records `THESEUS_HOP` for normal moves and `THESEUS_ICE_SLIDE` with waypoint data for ice slides
- Each feature file adds ~10 lines of event recording in its hook (on_enter, on_leave, on_push, on_environment_phase)

**Input buffer rules:**

- Buffer window is open during **any** animation phase (forward or reverse)
- Only fresh key-down events accepted (held keys ignored)
- Last press wins if multiple presses during window
- Bufferable actions: Move (N/S/E/W), Wait, Undo, Reset
- Pause is always immediate regardless of animation state
- When buffered input is pending, `dt` is multiplied by `g_settings.anim_speed` (default 2×, range 1×–4×)
- Buffered action is not pre-validated — game logic evaluates it on commit
- On death: movement blocked, but Undo and Reset still available
- Undo from death plays death animation in reverse (e.g. voxels reconstitute)

**Reverse undo animation:**

- `TurnRecord` is stored alongside each `UndoSnapshot` via `undo_store_turn_record()` after `turn_resolve()` fills it
- `anim_queue_start_reverse()` plays the record backward at 2× speed with reversed phase order (Mino2 → Mino1 → Env → Effects → Theseus)
- Grid restore (`undo_pop()`) is deferred until the reverse animation completes
- A "VHS rewind" visual overlay (blue tint + scan lines) renders during reverse playback
- Falls back to instant restore if no `TurnRecord` is available

**Verification:** Hook into puzzle scene — Theseus square slides smoothly between tiles instead of teleporting. Verify: (1) held key does not fire during animation, (2) fresh press during Minotaur's last step queues next turn, (3) no input accepted during Theseus/environment animations, (4) undo from death works. Additionally verify per-feature animations: ice slide shows hop then slide, teleport shows fade out/in, groove box push shows concurrent motion, environment phase shows sequential spike/turnstile/platform/conveyor animations.

---

## Step 4 — 3D Rendering Foundation

Switch from 2D rectangles to actual 3D voxel rendering. Freeform box placement, orthographic projection, depth buffer, basic lighting, baked ambient occlusion.

**Files:**

- `src/render/camera.h / .c` — Orthographic camera positioned for isometric-style diorama view. Handles viewport sizing (square sub-region on mobile). View + projection matrices.
- `src/render/voxel_mesh.h / .c` — Freeform box mesh builder. Accumulates axis-aligned boxes at arbitrary positions and dimensions into a single VBO (position + normal + color + uv per vertex = 12 floats). Boxes can be flagged `no_cull` for thin geometry (walls). API: `voxel_mesh_begin()`, `voxel_mesh_add_box(pos, size, color, no_cull)`, `voxel_mesh_build(cell_size)` → VAO/VBO + AO texture, `voxel_mesh_draw()`. During `build()`:
  1. Rasterizes all boxes into a coarse occupancy grid (~16 subdivisions per game tile)
  2. Emits vertices for each face, skipping fully occluded faces (unless `no_cull`)
  3. Packs visible faces into an AO texture atlas (8×8 texels per face)
  4. Raycasts 32 cosine-weighted hemisphere rays per texel for AO values
  5. Uploads geometry and AO texture (R8 format) to GPU; discards occupancy grid
- `src/render/ao_baker.h / .c` — Raytraced AO baker. Per-texel hemisphere raycasting against occupancy grid. Fibonacci hemisphere sampling, fixed-step ray marching, tangent-space basis from face normal.
- `src/render/occupancy_grid.h / .c` — Coarse boolean grid used during mesh build for face culling and AO raycasting. Allocated at `voxel_mesh_build()` time, populated by rasterizing all placed boxes, queried per face/texel, then freed.
- `src/render/lighting.h / .c` — Simple directional light + up to 8 point lights (lanterns). Passed as uniforms to the voxel shader.
- Voxel vertex/fragment shaders (embedded in `renderer.c`): position + normal + color + uv → AO texture sampled and multiplied into base color → lit output with half-Lambert diffuse + ambient + point lights.

**Verification:** ✅ Complete. Renders diorama with checkerboard floor, walls, actors. AO darkening at wall-floor junctions, hidden face culling, thin wall geometry preserved with `no_cull`.

---

## Step 5 — Procedural Diorama Generator

Generate full diorama meshes from level data + biome config, following the 12-step pipeline from design doc §09.

**Files:**

- `src/render/diorama_gen.h / .c` — Takes a `Grid*` + `BiomeConfig*` → populates `VoxelMesh` + returns `DioramaGenResult` with point lights. 12-step pipeline:
  1. Platform base (large box under grid with overhang)
  2. Floor tiles (2×2 paving stones per tile with mortar gaps, color jitter)
  3. Walls (stacked blocks: N×M per segment, mortar, per-block jitter, roughness)
  4. Back wall (tall north-edge wall, optional decoration prefabs)
  5. Doors (frame pillars + lintel, exit floor inlay with accent border)
  6. Impassable fill (prefabs or solid block fallback, random rotation)
  7. Feature markers (spike slits, pressure plates, teleporter rings, ice tint, crumble cracks)
  8. Floor decorations (seeded prefab scatter on walkable tiles)
  9. Wall decorations (prefabs on wall surfaces)
  10. Lantern pillars (at wall corners/endpoints, glow boxes, point lights)
  11. Exit light (amber beam boxes, warm point light)
  12. Edge border (ring of boxes around platform perimeter)
      AO baking happens automatically during `voxel_mesh_build()` (Step 4 raytraced AO textures).
      Uses seeded xorshift32 RNG (seed from `hash(level_id)`) for deterministic decoration.
- `src/data/biome_config.h / .c` — BiomeConfig struct hierarchy (palette, wall style, decorations, lanterns, prefabs). JSON loader via cJSON. `biome_config_defaults()` provides stone-labyrinth fallback.
- `assets/biomes/stone_labyrinth.json` — Default biome (warm sandstone)
- `assets/biomes/dark_forest.json` — Forest biome (dark greens, moss, mushrooms)

**Verification:** ✅ Complete. Puzzle scene renders procedural diorama with stacked-block walls, paving floors, decorations, and lantern point lights. Two biomes are visually distinct. Toggle 2D/3D with 'C'.

---

## Step 6 — Actor Rendering and Animation

Procedural actor models with deformable mesh system, proper animations with jelly-like deformations, and per-cause death animations with undo reversal. Broken into substeps that build on each other — each substep is independently testable.

---

### Step 6.1 — Deformable Mesh System ✅

Add vertex-shader-driven mesh deformation to the voxel rendering pipeline. Actor meshes are subdivided at build time to provide enough vertex density for smooth positional deformations. Deformation is controlled entirely by per-draw-call uniforms — zero additional memory, no CPU vertex rewriting.

**Rationale — precompute vs. dynamic:**

- **Mesh subdivision: precomputed.** Subdividing each face into a grid of smaller quads is purely geometric and never changes at runtime. Done once at mesh build time, stored in the existing VBO. Cost: ~150 vertices per actor (up from ~36) — trivial.
- **Deformation parameters: dynamic.** Squash, flare, lean, and directional squish vary frame-to-frame during animations. Expressed as ~7 float uniforms per draw call. The vertex shader applies the deformation, including corrected normals. No CPU vertex manipulation, no buffer re-upload.

**Files:**

- Update `src/render/voxel_mesh.h / .c`:
  - Add `voxel_mesh_set_subdivisions(VoxelMesh*, int subdivs)` — Sets the subdivision level for subsequent `add_box()` calls. Default 1 (no subdivision, current behavior). When set to e.g. 4, each box face emits a 4×4 grid of quads (5×5 = 25 vertices per face) instead of a single quad. Vertex positions, normals, UVs, and colors are interpolated across the subdivided grid. Subdivision happens during `add_box()` at build time — no runtime cost.
  - The occupancy grid, face culling, and AO baking continue to operate on the original box bounds (subdivision is purely a vertex density increase, not a geometry change).
- Update voxel vertex shader (embedded in `src/render/renderer.c`):
  - Add deformation uniforms:
    ```glsl
    uniform float u_deform_squash;       // Y-axis scale (1.0 = rest, <1 = compress, >1 = stretch)
    uniform float u_deform_flare;        // bottom XZ expansion (0.0 = none)
    uniform vec2  u_deform_lean;         // top-of-mesh XZ shear (lean direction)
    uniform vec2  u_deform_squish_dir;   // lateral compression direction (normalized)
    uniform float u_deform_squish;       // lateral compression amount (0.0 = none)
    uniform float u_deform_height;       // mesh height for normalizing Y (0 = disabled)
    ```
  - Deformation application order (all in model-local space, before model matrix):
    1. **Squash/stretch:** `pos.y *= squash`. XZ compensated for volume preservation: `pos.xz *= 1.0 / sqrt(squash)`.
    2. **Flare:** `float flare_t = 1.0 - pos.y / height; pos.xz *= 1.0 + flare * max(flare_t, 0.0)`. Strongest at Y=0 (bottom), zero at Y=height (top). With subdivided vertices at intermediate Y values, produces a smooth taper rather than a uniform bottom-face scale.
    3. **Lean:** `pos.xz += lean * (pos.y / height)`. Bottom stays planted, top shears in the lean direction.
    4. **Directional squish:** `float proj = dot(pos.xz, squish_dir); pos.xz -= squish_dir * proj * squish`. Compresses the mesh along an arbitrary horizontal direction (for pushing against boxes/walls).
  - **Normal correction:** Compute the inverse-transpose of the deformation Jacobian and transform normals accordingly. For these simple deformations, the Jacobian is analytically derivable:
    - Squash changes the Y component of normals (stretch normals in Y, shrink in XZ).
    - Flare tilts normals outward at the bottom.
    - Lean tilts normals opposite to the lean direction.
    - Squish tilts normals in the squish direction.
    - Renormalize after transformation.
  - When `u_deform_height == 0.0`, all deformation is skipped (identity path for non-actor geometry — diorama, decorations, etc.). This avoids branching cost for the vast majority of draw calls.

**Verification:** Set all deformation uniforms to identity (squash=1, flare=0, lean=0,0, squish=0, height=0) and verify rendering is pixel-identical to current. Then test each uniform independently with debug hotkeys: squash=0.7 should compress vertically and expand horizontally; flare=0.3 should bulge the bottom; lean=(0.2, 0) should tilt the top rightward. Normals should produce correct shading on deformed geometry (lit faces stay lit, shadowed faces stay shadowed).

---

### Step 6.2 — Actor Geometry Module ✅

Extract actor mesh generation from inline `puzzle_scene.c` code into a dedicated module. Replace the current single-box actors with multi-box compositions using subdivided meshes for deformation support.

**Files:**

- `src/render/actor_render.h / .c` — Actor mesh generation and rendering API:
  - `ActorParts` struct holds individual `VoxelMesh` components for each actor. Theseus: body (beveled blue cube). Minotaur: body (red cube), horns (white nubs on top face).
  - `actor_render_build_theseus(ActorParts*)` — Generates Theseus body mesh. ~40–50% tile size, RGB(80, 168, 251). Calls `voxel_mesh_set_subdivisions(&mesh, 4)` before adding boxes so each face has a 4×4 quad grid (~25 vertices per face). Composed of multiple boxes arranged so the body reads as a beveled cube. Each box is individually addressable for later death animation decomposition. Uses an invisible ground-plane occluder for AO baking (same technique as current code).
  - `actor_render_build_minotaur(ActorParts*)` — Generates Minotaur body mesh (~65% tile, RGB(239, 34, 34), true cube) with subdivision=4 for deformation support, and horn mesh (white nubs protruding from top, no subdivision needed — rigid). Each component is a separate `VoxelMesh` so horns can be independently transformed during roll animation.
  - `actor_render_destroy(ActorParts*)` — Cleans up all component meshes.
- Update `src/scene/puzzle_scene.c`:
  - Replace inline `voxel_mesh_begin/add_box/build` for actors with calls to `actor_render_build_*()`.
  - Store `ActorParts theseus_parts, minotaur_parts` instead of single `VoxelMesh theseus_mesh, minotaur_mesh`.
  - Rendering still uses simple model-matrix positioning (same as current) — deformation uniforms set to identity for now. Animation changes come in later substeps.
  - Before each actor draw call, set `u_deform_height` to the actor's mesh height. After drawing, reset to 0.0 to disable deformation for subsequent non-actor draws.
- Update `src/render/README.md` — Add actor_render entry.

**Verification:** Game looks identical to current state — actors are the same shape and color, shadows work, AO darkening on bottom edges present. The only change is code organization, richer Minotaur geometry (horns visible at rest), and subdivided actor meshes ready for deformation.

---

### Step 6.3 — Theseus Hop Deformation ✅

Full deformation chain for Theseus's hop animation: anticipation squat → jump elongation → lean → landing flare → damped wobble settle. All deformation is expressed by driving the shader uniforms from Step 6.1 — no mesh changes.

**Changes:**

- Add `DeformState` struct to `src/render/actor_render.h`:
  - Holds all deformation parameters: `squash`, `flare`, `lean_x`, `lean_z`, `squish_dir_x`, `squish_dir_z`, `squish_amount`.
  - Helper: `deform_state_identity()` → all values at rest (squash=1, everything else 0).
  - Helper: `deform_state_apply(const DeformState*, GLuint shader)` → sets the uniform values.
- Update `src/scene/puzzle_scene.c` Theseus rendering section:
  - Compute `DeformState` each frame based on hop progress (`thop` from `anim_queue_theseus_pos()`):
    - **Anticipation squat** (hop progress 0.0–0.1): Before liftoff, brief downward compression. `squash = 0.85`, `flare = 0.15`. Creates a "gathering energy" crouch. Requires the anim_queue to expose a small pre-hop window, or the first ~10% of the hop tween is repurposed as the squat phase (Theseus hasn't left the ground yet).
    - **Jump elongation** (progress 0.1–0.5): Theseus stretches vertically as he rises. `squash = 1.15` at peak, tapering smoothly. Volume preservation makes him slightly narrower — reads as "shooting upward."
    - **Lean** (progress 0.1–0.9): Top of mesh shears in the movement direction. `lean = movement_dir * 0.12` at liftoff, easing to 0 at apex, then `lean = -movement_dir * 0.06` approaching landing (feet-first). Movement direction derived from tween start/end positions.
    - **Landing flare** (progress 0.85–1.0): Bottom expands on impact. `squash = 0.82`, `flare = 0.25`. The subdivided mesh makes this visibly taper — bottom 30% bulges, top stays compact.
    - **Damped wobble** (post-hop, ~0.20s): After the anim_queue transitions past the Theseus phase, a `wobble_timer` ticks down. During wobble: `squash = 1.0 + amplitude * sin(timer * freq) * exp(-timer * damping)`. Parameters are tunable: `amplitude = 0.12`, `freq = 35.0`, `damping = 12.0`. The oscillation settles to rest naturally. `flare` tracks `(1.0 - squash) * 0.5` so the bottom subtly breathes with each wobble cycle.
  - The deformation chain applies to all Theseus hop variants (normal hop, ice-slide initial hop, teleport-in landing). During ice-slide linear motion, lean and elongation are suppressed — only a subtle squat persists (see Step 6.7).

**Verification:** Theseus visibly crouches before jumping, stretches in the air, leans into the hop direction, flares at the bottom on landing, and wobbles briefly to rest. The effect is lively but not cartoonish — dial down amplitudes if it reads as too rubbery. Verify with hops in all four directions. Verify the wobble settle looks natural (no abrupt snap to rest). Verify ice-slide doesn't apply hop deformations during the sliding portion.

---

### Step 6.4 — Minotaur Roll Animation ✅

Replace the Minotaur's current hop-slide with a 90° rolling motion. The Minotaur rotates around its leading bottom edge, with a slight upward arc for power feel. Landing uses the deformable mesh for impact squash.

**Changes:**

- Update `src/engine/anim_queue.h / .c`:
  - Add `mino_dir_x` and `mino_dir_y` (ints: -1/0/+1) to `AnimQueue` — the direction of each Minotaur step, set when the minotaur phase begins. Needed by the renderer to determine the roll axis and pivot edge.
  - Expose `anim_queue_minotaur_dir(const AnimQueue*, int* dx, int* dy)` query function.
- Update `src/scene/puzzle_scene.c` Minotaur rendering:
  - **Roll transform:** Instead of a simple translate, compute a rotation around the leading bottom edge. The pivot point is at the Minotaur's base, offset by half-size in the movement direction. The rotation axis is perpendicular to movement (e.g., moving east → rotate around Z axis at the east bottom edge). Rotation angle: 0° → 90° over the step duration.
  - **Arc lift:** Add a small upward offset during the roll (parabolic, peak ~0.08 at midpoint) so the Minotaur doesn't clip through the floor during rotation. This gives the "jump-roll" power feel.
  - **Landing deformation:** On roll completion (progress 0.85–1.0), apply `squash = 0.88`, `flare = 0.15` to the Minotaur's deform state. Followed by a short damped wobble (~0.15s, `amplitude = 0.08`, `freq = 25.0`, `damping = 15.0`) — heavier and less bouncy than Theseus. Conveys weight and impact.
  - **Anticipation squat:** Before the roll begins (progress 0.0–0.08), brief `squash = 0.92` — a subtle "loading" compression before launching into the roll.
  - **Horn retraction/extension:** During anticipation (progress 0.0–0.10), horn meshes scale Y toward 0 (retract into body). Horns stay fully retracted during the entire roll since the cube is rotating and they would point sideways. When the Minotaur comes to rest, the post-roll wobble provides a natural moment for horns to pop back out.
  - The Minotaur is a **true cube** (height = width = depth), so a 90° roll lands on an identical face — no rotation unwinding or "landing phase" is needed. The roll simply completes at 90° and the cube looks the same.
  - When the Minotaur is idle (not animating), horns are on top — this is the rest pose regardless of how many rolls have occurred.

**Verification:** Minotaur visibly rolls (tumbles) from tile to tile instead of sliding. Landing produces a satisfying squash + wobble that conveys weight. Horns retract during roll and re-emerge on top at rest. Roll direction is correct for all four cardinal directions. Two-step turns show distinct rolls with a pause between. Reverse (undo) plays the roll backward correctly.

---

### Step 6.5 — Ground Shake on Minotaur Landing ✅

Add a localized screen shake when the Minotaur lands after each roll step.

**Changes:**

- Update `src/scene/puzzle_scene.c`:
  - Add `shake_timer`, `shake_intensity`, `shake_offset_x`, `shake_offset_y` fields to `PuzzleScene`.
  - When the Minotaur step animation completes (anim phase transitions from MINOTAUR_STEP1→STEP2 or STEP2→next), trigger shake: set `shake_timer = 0.12`, `shake_intensity = 0.015` (world-space units).
  - Each frame during shake: compute dampened sinusoidal offset (`intensity * sin(timer * 60) * timer/0.12`), apply as translation offset to the view matrix before rendering the diorama. The offset decays to zero over the shake duration.
  - Death-by-squish (Step 6.10) uses a stronger shake (`intensity = 0.025`).

**Verification:** Subtle screen jitter when Minotaur lands. Effect is brief and localized-feeling. Does not interfere with gameplay readability. Two successive Minotaur steps produce two distinct shakes.

---

### Step 6.6 — Contextual Deformations: Push, Turnstile, and Collision ✅

Deformation effects for Theseus interacting with pushable objects and walls. Uses directional squish (compression along the push axis) and bounce-back recovery.

**Changes:**

- Update `src/scene/puzzle_scene.c`:
  - **Groove box push (successful):** During the push sub-phase (Theseus pressed against the box, both moving), apply directional squish: `squish_dir = movement_direction`, `squish_amount = 0.12`. Theseus compresses slightly in the push direction as if straining against the box. On push completion, release with a brief elastic recovery (squish goes 0.12 → -0.04 → 0 over ~0.10s).
  - **Failed groove box push (bump):** The existing bump animation (approach → linger → return) already handles positioning. Add deformation on top: during the approach phase, ramp `squish_amount` up to 0.18 in the push direction (Theseus compresses harder against the immovable box). During linger, add a subtle oscillation in squish. During return, release with elastic bounce-back. This makes the "bonk" feel physical rather than just positional.
  - **Manual turnstile push:** Same squish pattern as groove box push — Theseus compresses against the wall he's pushing. `squish_amount = 0.10`, lighter than groove box since the turnstile is lighter.
  - **Ice slide wall collision:** When Theseus slides on ice and hits a wall, apply a strong directional squish on impact (`squish_amount = 0.20`), followed by a damped wobble as he reconstitutes on the tile. The squish direction is the slide direction. This reuses the same elastic recovery as failed groove box push but with higher initial amplitude.

**Verification:** Push a groove box — Theseus visibly compresses against it during the push. Try to push an immovable box — Theseus squishes harder and bounces back. Push a turnstile — subtle compression. Slide into a wall on ice — satisfying squish on impact. All effects recover cleanly to rest pose.

---

### Step 6.7 — Contextual Deformations: Teleport, Ice Slide, and Pit Fall ✅

Deformation effects for special movement types that have distinct visual character.

**Changes:**

- Update `src/scene/puzzle_scene.c`:
  - **Teleport beam-up (departure):** During teleport-out phase (scale-down/fade-out), replace the current uniform scale with vertical elongation: `squash` ramps from 1.0 → 2.5 over the phase duration while XZ scale shrinks to ~0.3 (via volume preservation). Theseus stretches tall and thin like a Star Trek transporter beam — "pulled upward" into a column before vanishing. The existing fade/alpha still applies on top.
  - **Teleport reconstitution (arrival):** During teleport-in phase (scale-up/fade-in), reverse the effect: start at `squash = 2.5` (tall thin column) and contract to 1.0 as Theseus materializes. Follow with the standard landing flare + wobble from Step 6.3. Creates a "beam down and solidify" feel.
  - **Ice slide squat:** During the ice-slide linear motion sub-phase (after the initial hop), apply a persistent mild squat: `squash = 0.90`, `flare = 0.08`. Theseus rides low and compact while sliding, conveying speed and lack of control. Lean is suppressed (no directional tilt during slide — he's sliding, not hopping).
  - **Ice slide to normal tile transition:** When the ice slide ends on a non-ice tile, the squat transitions into a hop to the normal tile. The squat releases into the standard hop deformation chain from Step 6.3 (elongation → lean → landing flare → wobble).
  - **Crumbling floor fall elongation:** When Theseus falls into a pit (before death voxel decomposition), apply dramatic vertical elongation: `squash` ramps from 1.0 → 3.0 as he drops, creating a Wile E. Coyote "taffy stretch" downward. XZ shrinks via volume preservation. This plays as a brief pre-decomposition effect (~0.15s) before the death animation framework takes over with individual voxel particles. The elongation direction is downward (negative Y stretch), with the top of the mesh anchored so it looks like he's being pulled down.

**Verification:** Teleport looks like a transporter beam — Theseus stretches into a column, vanishes, then reconstitutes at the destination with a satisfying wobble. Ice slide shows Theseus riding low and compact. Ice-to-normal transition produces a smooth squat-to-hop handoff. Crumbling floor shows a comedic stretch before voxel scatter. All effects reverse correctly during undo.

---

### Step 6.8a — Deferred Win Condition + Forced Exit ✅

Win condition logic and the forced exit-hop animation. The win is **deferred** (see 01-core-mechanics.md §7.4): Theseus reaching the exit tile does **not** end the level immediately. The full turn cycle completes (Environment Phase → Minotaur Phase), and the win is only confirmed at the **start of Theseus's next turn** if he is still alive on the exit tile. This ensures the Minotaur always gets his two steps.

**Win condition logic (in `turn_resolve()` / puzzle scene):**

- When Theseus moves onto the **exit tile** (the interior tile adjacent to the exit door opening), set a pending flag (e.g. `grid.theseus_on_exit = true`) but do **not** return `TURN_RESULT_WIN` yet. The Environment and Minotaur phases still resolve normally.
- If the Minotaur kills Theseus during this turn (captures him on the exit tile), the result is a normal loss — no win.
- At the **start of the next turn** (before accepting player input), if `theseus_on_exit` is true and Theseus is alive, **then** the win triggers:
  - Theseus is **forced to take a normal step** in the exit-door direction, hopping from the exit tile onto the **virtual exit tile** outside the grid. This is automatic, not player-controlled. It uses the standard hop animation (normal arc, normal deformation).
  - Immediately after Theseus passes through the exit opening, the **exit door closes and locks behind him** — same visual treatment as the locking-gate feature (bars/gate rise up in the doorway). This seals Theseus safely outside the grid.
  - No Environment Phase or Minotaur Phase occurs for this forced step.
- The turn counter from the previous turn (when Theseus reached the exit tile) is the **final move count**. The forced step out does not increment the counter.
- Input is blocked once the win triggers; the forced step plays automatically.

**Exit direction:**

The exit tile is always on a border of the grid. Theseus's forced step goes **outward** through the exit door opening:

- **Left-wall exit:** Theseus hops westward (screen-left) onto the virtual tile.
- **Right-wall exit:** Theseus hops eastward (screen-right) onto the virtual tile.
- **Top (back) wall exit:** Theseus hops northward (away from camera) onto the virtual tile.
- There are never south-wall exits (per §7.1).

**Animation sequence:**

1. **Forced exit hop (~0.15s):** Standard hop animation from the exit tile to the virtual tile in the exit direction. Normal parabolic arc, normal squash/stretch deformation. This reads as a regular move — the celebration hasn't started yet.
2. **Exit door locks (~0.3s, overlaps with end of hop):** As Theseus clears the doorway, the exit door closes behind him using the locking-gate animation (bars/gate rise from floor). Same visual system as `locking_gate` feature. Once sealed, the Minotaur is visually cut off from Theseus.
3. After the gate locks, the game **freezes** with Theseus standing on the virtual tile. Victory celebration is added in Step 6.8b.

**Changes:**

- Update `src/engine/anim_queue.h / .c`:
  - Add `ANIM_PHASE_WIN_EXIT` for the forced exit hop. Duration: ~0.15s (same as normal hop).
  - Add `ANIM_PHASE_WIN_GATE` for the exit door lock animation. Duration: ~0.3s.
  - `anim_queue_start()` detects `record.result == TURN_RESULT_WIN` and appends the two win phases after all normal phases complete. Passes `exit_direction` (derived from exit tile grid position).
  - Expose `anim_queue_win_phase(const AnimQueue*)` → current win sub-phase enum (EXIT, GATE, NONE).
  - Expose `anim_queue_win_exit_dir(const AnimQueue*)` → direction enum (WEST, EAST, NORTH).
- Update `src/game/turn.h / .c` (or equivalent turn resolution):
  - Add `theseus_on_exit` flag to `Grid` (or turn state). Set when Theseus moves onto the exit tile; do not short-circuit the turn.
  - Check `theseus_on_exit` at start-of-turn to produce `TURN_RESULT_WIN` before accepting input.
- Update `src/scene/puzzle_scene.c`:
  - At start of each turn, before accepting input, check for deferred win and trigger win animation if applicable.
  - During `ANIM_PHASE_WIN_EXIT`: standard hop tween from exit tile to virtual tile in exit direction.
  - During `ANIM_PHASE_WIN_GATE`: trigger exit door locking-gate animation (reuse locking-gate visual system).
  - After gate phase completes, block further input (celebration added in 6.8b).

**Verification:** The Minotaur always completes his moves before the win is confirmed — stepping onto the exit tile is not a free escape. The forced exit hop looks like a normal move. The exit door locks behind Theseus with the locking-gate visual. After the gate locks, Theseus stands idle on the virtual tile and the game is frozen (no input accepted, no further turn resolution). Winning on a level where the Minotaur can reach the exit tile in two steps results in a loss, not a win.

---

### Step 6.8b — Victory Celebration Loop ✅

Looping in-place celebration bounce on the virtual exit tile. Two variants based on whether the player achieved the level's `optimal_turns` (from the level JSON). Builds on 6.8a — starts after the exit door locks.

**Animation:**

- **Victory celebration (loops indefinitely):** Theseus hops **in place** on the virtual tile in a repeating celebratory bounce. Each bounce is a parabolic arc (higher apex than normal — peak ~0.3 units) with exaggerated squash/stretch deformation (range 0.75–1.25). The bounce loops continuously until externally stopped (by 6.8c).
  - **Optimal variant (player achieved `optimal_turns`):** Each in-place hop includes a full **360° Y-axis spin**. The spin is timed to complete exactly once per hop cycle, so Theseus twirls excitedly on every bounce. Increase emissive glow intensity (blue brightening to ~1.5×).
  - **Non-optimal variant:** No spin — Theseus bounces in place with the exaggerated arc. Still celebratory, but more restrained.

**Changes:**

- Update `src/engine/anim_queue.h / .c`:
  - Add `ANIM_PHASE_WIN_CELEBRATE` after `ANIM_PHASE_WIN_GATE`. This phase does **not** have a fixed duration — it loops until externally signaled to stop.
  - `anim_queue_start()` now also appends the celebrate phase after the gate phase. Passes `is_optimal` flag (comparing turn count against level's `optimal_turns`).
  - Expose `anim_queue_win_is_optimal(const AnimQueue*)` → bool.
  - Expose `anim_queue_win_stop_celebrate(AnimQueue*)` — called by puzzle scene to end the loop (used in 6.8c).
  - Update `anim_queue_win_phase()` to include CELEBRATE in the sub-phase enum.
- Update `src/scene/puzzle_scene.c`:
  - During `ANIM_PHASE_WIN_CELEBRATE`: loop in-place bounce on virtual tile. If optimal, apply 360° Y-axis spin per hop cycle and increased emissive intensity. If non-optimal, bounce only.

**Verification:** After the exit door locks (6.8a), Theseus immediately begins bouncing in place. Optimal solution triggers 360° spins on each bounce; non-optimal bounces without spinning. The bounce loops indefinitely without visual glitches (smooth looping at the seam between cycles). Emissive glow is visibly brighter on optimal variant.

---

### Step 6.8c — Victory Overlay + Camera Transition

Results UI overlay and camera pan to the next level's diorama. Theseus continues bouncing (from 6.8b) throughout. Old scene is torn down off-camera.

**Sequence:**

1. **Victory overlay:** Once the celebrate phase begins (6.8b), the results overlay fades in (turn count vs. optimal, stars earned, etc.). Theseus **continues hopping** behind/beneath the overlay.
2. **Camera transition to next level:** When the player dismisses the results (or after a timeout), the camera begins zooming/panning to the next level's diorama position. Theseus **continues hopping in place** during this transition. The next level diorama is positioned off-camera, so by the time the camera arrives at the new location, the old puzzle geometry, actors, and Theseus can be **torn down invisibly** — the player never sees them disappear.
3. **Teardown + next level:** Once the old diorama is fully off-camera, call `anim_queue_win_stop_celebrate()` and destroy the old puzzle scene state. The new level's entrance sequence (§7.5) begins once the camera settles on the new diorama.

**Changes:**

- Add `src/scene/victory_overlay.h / .c` (or integrate into puzzle scene):
  - Renders results: turn count, optimal turn count, stars (1 for completion, 2 if turns ≤ optimal).
  - Fade-in animation over ~0.3s.
  - Accepts dismiss input (confirm button / tap) to trigger camera transition.
- Update `src/scene/puzzle_scene.c`:
  - Show victory overlay once `ANIM_PHASE_WIN_CELEBRATE` begins. Keep Theseus bouncing behind it.
  - On results dismiss / next-level trigger: begin camera pan to next diorama.
  - When old diorama is off-camera: call `anim_queue_win_stop_celebrate()`, tear down old puzzle state (grid, actors, diorama meshes).
  - Hand off to next level load / entrance sequence.

**Verification:** Results overlay appears while Theseus bounces. Stars and turn count display correctly. Dismissing results starts the camera pan. Theseus keeps bouncing during the pan. Old scene teardown happens off-camera — the player never sees geometry pop out of existence. New level entrance sequence begins cleanly after the camera arrives.

---

### Step 6.9a — Death Animation Framework (Core Physics) ✅

Tile environment query layer, basic death voxel decomposition, gravity with floor-height awareness, and pit fall-through. Uses simple lerp reversal as a placeholder — enough to visually verify the system before adding wall collisions and keyframe recording. Death voxels are rigid (no deformation shader) — the deformable mesh is only for the intact actor.

**Files:**

- `src/game/tile_physics.h / .c` — Tile environment query helper for voxel physics:
  - `TileSurface` struct: `float surface_y` (floor height at this tile), `bool is_pit` (bottomless — voxels fall through), `bool is_impassable` (solid obstacle — voxels cannot enter).
  - `tile_physics_query(const Grid*, const BiomeConfig*, int col, int row)` → `TileSurface` — Returns the physical properties of a tile for voxel scatter simulation. Accounts for all terrain types:
    - **Normal floor:** `surface_y = 0.0`, not pit, not impassable.
    - **Conveyor/auto-turnstile:** `surface_y = CONVEYOR_HEIGHT` (0.10). Voxels landing here rest at the elevated surface.
    - **Groove trench tiles:** `surface_y = -trench_depth` (default −0.07). Voxels can fall into and roll along the trench channel. Uses `groove_box_get_path()` to identify groove tiles and reads `biome->groove_trench.trench_depth`.
    - **Collapsed crumbling floor / moving-platform pit:** `is_pit = true`. Voxels that enter these tiles fall through and vanish (scale → 0 as Y decreases below −0.5).
    - **Impassable (solid block):** `is_impassable = true`. Voxels treat these the same as walls — bounce off the tile boundary. (Actual bounce physics deferred to 6.9b; in this step impassable tiles are queried but not yet used for collision response.)
    - **Out-of-bounds:** `is_pit = true`. Voxels that scatter off the grid edge fall into the void.
  - `tile_physics_has_wall(const Grid*, int col, int row, Direction side)` → `bool` — Thin wrapper around `grid_has_wall()` that also returns true for impassable neighbor tiles (since their boundary acts as a wall for voxel scatter). Defined here for use in 6.9b.
  - `tile_physics_wall_x(const Grid*, int col, int row, Direction side)` → `float` — Returns the world-space X or Z coordinate of the wall plane on the given side of the tile, accounting for `WALL_THICKNESS` (0.20). Defined here for use in 6.9b.
- Update `src/game/README.md` — Add tile_physics entry.
- `src/render/death_anim.h / .c` — Death animation system (core):
  - `DeathVoxel` struct: position (vec3), velocity (vec3), rotation (vec3), angular_velocity (vec3), scale (vec3), color (vec4), original_position (vec3), original_color (vec4), `bool fallen` (true if voxel fell into a pit and is no longer visible). (Keyframe fields added in 6.9b.)
  - `DeathAnim` struct: array of `DeathVoxel` (max ~64), count, timer, duration, type enum (`DEATH_SQUISH`, `DEATH_WALK_INTO`, `DEATH_SPIKE`, `DEATH_PETRIFY`, `DEATH_PIT_FALL`), `finished` flag, `reversing` flag. Stores `const Grid*` and `const BiomeConfig*` pointers for tile queries during simulation.
  - `death_anim_init(DeathAnim*, DeathType, const ActorParts*, float actor_x, float actor_z, const Grid*, const BiomeConfig*)` — Decomposes the actor's body mesh into individual `DeathVoxel` particles at their current world positions. Sets initial velocities/rotations based on death type.
  - `death_anim_update(DeathAnim*, float dt)` — Advances all voxel particles with physics:
    - **Gravity:** `velocity.y += gravity * dt` (gravity ≈ −9.8 world-units/s²).
    - **Integration:** `position += velocity * dt`, `rotation += angular_velocity * dt`.
    - **Floor clamping:** Query `tile_physics_query()` at voxel's current (col, row). If `position.y ≤ surface_y`, clamp to `surface_y`, reflect `velocity.y` with elasticity (coefficient ≈ 0.3–0.4), dampen `angular_velocity` by 0.7×.
    - **Pit fall-through:** If `tile_physics_query()` returns `is_pit` at the voxel's tile, skip floor clamping — voxel falls freely. When `position.y < -0.5`, begin shrinking `scale` toward 0. When `scale < 0.01`, mark `fallen = true` and stop updating.
    - **Floor-height transitions:** When a voxel crosses from one tile to another with a different `surface_y`, it naturally rolls off or onto the height difference (no special case needed — gravity + the new surface_y handle it). Groove trenches act as channels that collect and guide rolling voxels.
    - **Rest detection:** When a voxel's speed (linear + angular) drops below a threshold for 0.1s, freeze it in place.
    - **No wall collision yet** — voxels pass through walls in this step. Wall bounce added in 6.9b.
  - `death_anim_render(const DeathAnim*, GLuint shader)` — Renders each non-fallen `DeathVoxel` as an individually transformed box with `u_deform_height = 0.0` (deformation disabled). Uses a shared unit-cube VBO (generated once) with per-particle model matrix containing position, rotation, and scale.
  - `death_anim_start_reverse(DeathAnim*)` — Placeholder reversal: interpolates all voxels from current position/color back toward `original_position`/`original_color` over `duration * 0.5` seconds (simple lerp). Fallen voxels reappear at their last known position above the pit. (Replaced with keyframe-accurate reversal in 6.9b.)
  - `death_anim_is_finished(const DeathAnim*)` — True when forward or reverse playback is done.
  - `death_anim_destroy(DeathAnim*)` — Cleanup.
- Update `src/scene/puzzle_scene.c`:
  - Add `DeathAnim death_anim` field to `PuzzleScene`.
  - When `turn_result` is a loss, after the relevant animation phase completes, initialize the death animation. Suppress normal Theseus rendering while death anim is active. Reset deformation uniforms to identity.
  - When undo is triggered during death state, call `death_anim_start_reverse()`. Defer grid restore until reverse completes. Resume normal Theseus rendering after reverse finishes.
- Update `src/render/README.md` — Add death_anim entry.

**Verification:** Framework compiles and links. A test death (e.g., collision with Minotaur) triggers the decomposition — Theseus breaks into individual blue boxes that fall with gravity. Voxels settle at correct floor heights (roll into groove trenches, rest on conveyor surfaces) and fall through collapsed/pit tiles into the void. Voxels pass through walls (expected — wall collision comes in 6.9b). Undo lerps voxels back to their original positions and reassembles Theseus. Specific scatter patterns come in subsequent substeps.

---

### Step 6.9b — Death Animation Framework (Wall Collision + Keyframe Undo) ✅

Wall-bounce collision response and keyframe-based trajectory recording for frame-accurate undo reversal. Replaces the placeholder lerp reversal from 6.9a.

**Changes:**

- Update `src/render/death_anim.h / .c`:
  - **Keyframe recording:**
    - Add `VoxelKeyframe` struct: `float time`, `vec3 position`, `vec3 velocity`, `vec3 rotation`, `vec3 angular_velocity`. Captured at initialization and at every collision event (wall bounce, floor bounce, pit entry, rest).
    - Add `VoxelKeyframe keyframes[MAX_VOXEL_KEYFRAMES]` and `int keyframe_count` to `DeathVoxel`. `MAX_VOXEL_KEYFRAMES = 8`. If a voxel exceeds 8 collisions, its velocity is killed (it stops in place and records a final keyframe).
  - **Wall collision in `death_anim_update()`:**
    - Each physics substep, check if the voxel's XZ trajectory crosses a wall boundary via `tile_physics_has_wall()`. On collision: reflect the velocity component perpendicular to the wall with elasticity (coefficient ≈ 0.5–0.6), apply slight angular impulse from the impact. Use `tile_physics_wall_x()` for precise contact position so the voxel doesn't clip through. Record keyframe at bounce time.
    - Impassable neighbor tiles act as walls (already handled by `tile_physics_has_wall()`).
  - **Keyframe recording for all collision types:**
    - Floor bounce: record keyframe at each ground impact.
    - Pit entry: record keyframe when voxel crosses into a pit tile.
    - Rest: record final keyframe when voxel freezes.
    - Init: record keyframe at time=0 with starting state.
  - **Replace `death_anim_start_reverse()`:**
    - Remove lerp reversal. New implementation walks each voxel's keyframe array in reverse order, interpolating position/velocity/rotation between keyframes. Fallen voxels reappear (scale back up from 0) at their pit-entry keyframe. Total reverse duration = `duration * 0.5` seconds (time-remapped to traverse all keyframes proportionally).

**Verification:** Trigger a death near walls — voxels bounce off with visible elasticity, angular spin changes on impact, and settle realistically. Trigger a death near a pit — some voxels fall through, others bounce off adjacent walls and stay on the board. Undo from any death: voxels retrace their exact collision paths in reverse (rising out of pits, bouncing off walls backward, reassembling into Theseus). Fast-forward through a death + undo cycle to verify keyframe playback at accelerated speed.

---

### Step 6.10 — Death: Minotaur Squish ✅

The Minotaur rolls onto Theseus's tile and crushes him. Voxels flatten and scatter outward.

**Changes:**

- Update `src/render/death_anim.c`:
  - `DEATH_SQUISH` initialization: Compute scatter direction for each voxel as radially outward from tile center. Initial velocity: strong horizontal outward + brief upward arc. Apply vertical scale compression (squash Y to ~0.2) over the first 0.15s, then release to tumble. The Minotaur's roll direction influences the bias of the scatter (more voxels scatter away from the roll direction).
  - Duration: ~0.6s (scatter + settle).
- Update `src/scene/puzzle_scene.c`:
  - Trigger `DEATH_SQUISH` when `turn_result == TURN_RESULT_LOSS_COLLISION` and the Minotaur moved onto Theseus (Minotaur phase was active).
  - Apply stronger ground shake on death impact (`shake_intensity = 0.025`).

**Verification:** Minotaur rolls onto Theseus → blue voxels squash flat then scatter outward and settle on the floor. Ground shakes on impact. Undo reverses: voxels slide back and reassemble, Minotaur rolls backward.

---

### Step 6.11 — Death: Walk Into Minotaur

Theseus hops toward the Minotaur and shatters on impact. The hop animation cuts short mid-arc.

**Changes:**

- Update `src/render/death_anim.c`:
  - `DEATH_WALK_INTO` initialization: Voxels scatter backward (away from Minotaur, opposite to movement direction). Higher initial velocity than squish — feels like bouncing off a wall. Some upward scatter, some lateral spread.
  - Duration: ~0.5s.
- Update `src/scene/puzzle_scene.c`:
  - Trigger `DEATH_WALK_INTO` when `turn_result == TURN_RESULT_LOSS_COLLISION` and Theseus moved onto the Minotaur (Theseus phase was active).
  - The hop animation should terminate at ~60% progress (mid-arc), at which point the death animation begins at Theseus's mid-hop position. This requires checking for collision during the Theseus phase and interrupting the hop tween.
  - Minotaur recoil: brief scale pulse (1.0 → 1.05 → 1.0 over 0.15s) applied to the Minotaur's model matrix.

**Verification:** Theseus hops toward Minotaur, collides mid-air, shatters backward. Minotaur pulses. Undo reverses the shatter and hop.

---

### Step 6.12 — Death: Spike Impale

Spikes shoot up through Theseus, launching voxels upward.

**Changes:**

- Update `src/render/death_anim.c`:
  - `DEATH_SPIKE` initialization: Voxels launch primarily upward (strong +Y velocity) with slight horizontal scatter. A few voxels (2–3) are flagged as "impaled" — they travel upward to spike-tip height then stop, remaining visible on the spike geometry.
  - Duration: ~0.7s (upward launch + fall + settle).
- Update `src/scene/puzzle_scene.c`:
  - Trigger `DEATH_SPIKE` when `turn_result == TURN_RESULT_LOSS_HAZARD` and the cause is spike activation (need to check event type in the turn record).
  - Impaled voxels persist after the death animation finishes (rendered as static decoration on the spike tips until undo).

**Verification:** Spikes activate → Theseus voxels pop upward and scatter, a few remain impaled on spike tips. Undo: voxels fall back onto spikes, slide down, reassemble.

---

### Step 6.13 — Death: Medusa Petrification

Theseus turns to stone, freezes, cracks, and crumbles.

**Changes:**

- Update `src/render/death_anim.c`:
  - `DEATH_PETRIFY` is a multi-phase death animation:
    1. **Color wave** (0.0–0.3s): Sweep grey color across voxels from the Medusa's direction. Each voxel transitions blue → grey based on its distance from the Medusa-facing edge. Theseus freezes mid-lean (the lean from the hop that triggered death).
    2. **Freeze hold** (0.3–0.5s): Static pause — fully grey, frozen pose.
    3. **Crumble** (0.5–0.9s): Crack lines appear (thin dark voxels inserted between body voxels), then voxels collapse downward into a rubble pile with slight horizontal spread. No bounce — heavy stone-like fall.
  - Duration: ~0.9s.
  - Requires Medusa direction as initialization parameter (which side the Medusa faces from), used for color sweep direction and lean freeze angle.
- Update `src/scene/puzzle_scene.c`:
  - Trigger `DEATH_PETRIFY` when the death cause is Medusa (check `ANIM_EVT_THESEUS_HOP` with `TURN_RESULT_LOSS_HAZARD` and Medusa feature on the source tile).
  - Pass Medusa facing direction from the turn record or feature data.

**Verification:** Theseus turns grey in a wave from Medusa direction → freezes → cracks and crumbles into rubble pile. Undo: rubble lifts, reassembles, grey sweeps back to blue.

---

### Step 6.14 — Death: Pit Fall

Theseus falls into a pit (crumbling floor, missing platform, bottomless void). The elongation deformation from Step 6.7 plays as a pre-decomposition effect before the death voxels take over.

**Changes:**

- Update `src/render/death_anim.c`:
  - `DEATH_PIT_FALL` initialization: Voxels drop coherently as a group (not scattered) with slight tumble rotation. All voxels shrink toward a vanishing point below the diorama platform (scale decreases to 0 as Y decreases). No horizontal scatter — straight down.
  - For lava variant: voxels shift color blue → orange → red during descent, with additive glow (increase alpha or emissive). Scale dissolves to 0 faster.
  - Duration: ~0.8s (not counting the ~0.15s elongation pre-phase from Step 6.7).
- Update `src/scene/puzzle_scene.c`:
  - Trigger sequence: elongation deformation plays on the intact mesh (~0.15s, `squash` ramps 1.0 → 3.0), then the mesh is decomposed into death voxels which continue the fall.
  - Trigger `DEATH_PIT_FALL` when the death cause is crumbling floor, missing platform, or pit tile.
  - The pit opening should remain visible (dark void rendered in the floor) after Theseus falls.

**Verification:** Floor crumbles → Theseus stretches downward (comedic elongation) → decomposes into voxels that drop into the void, shrinking and tumbling. Undo: voxels rise, reconstitute, Theseus un-stretches, floor reassembles. Lava variant: voxels glow orange/red during descent.

---

### Step 6 — Overall Verification

After all substeps are complete, perform a full integration test:

1. **Deformable mesh:** All deformation primitives (squash, flare, lean, directional squish) work independently and in combination. Normals are correctly adjusted (lighting follows deformation). Non-actor geometry is unaffected.
2. **Movement animations:** Theseus hops with full deformation chain (anticipation → elongation → lean → landing flare → wobble) in all directions. Minotaur rolls with landing squash + wobble, horn retract/extend, and ground shake. Both work correctly during two-step Minotaur turns.
3. **Contextual deformations:** Push squish against groove boxes and turnstiles. Failed push bounce-back. Ice slide squat with wall collision squish. Teleport beam-up/down elongation. Pit fall stretch.
4. **Win:** Theseus spins, glows, and bounces at exit, then results appear.
5. **All five death types:** Each triggers the correct animation based on death cause. Visual effects are distinct and readable.
6. **Undo from death:** Every death animation reverses smoothly — voxels reconstitute, Theseus is playable again.
7. **Input buffering:** Animations play correctly when fast-forwarded (buffered input pending). Undo buffering during death state works.
8. **Reverse playback:** All movement and deformation animations reverse correctly during undo rewind. Deformation state recovers to identity on reverse completion.

---

## Step 7 — Post-Processing and Lighting Polish

Visual polish pass per design doc §02.

**Files:**

- `src/render/post_process.h / .c` — Render to FBO, then fullscreen quad with:
  - Vignette (darken edges)
  - Bloom on emissive surfaces (lantern glow, exit god-light)
- Update `src/render/lighting.c` — Dynamic point lights for lanterns (cool cyan/blue) and exit (warm amber). Soft ambient from above.
- Shadow pass (simple shadow map or blob shadows under actors)

**Verification:** Lanterns emit visible glow, exit door has warm light beam, screen edges have subtle vignette, no visual artifacts.

---

## Step 8 — HUD Overlay

In-game HUD rendered over the 3D scene during puzzle play.

**Files:**

- Update `src/scene/puzzle_scene.c` — HUD layer:
  - Turn counter (always visible, top area)
  - Level name (always visible)
  - Undo/Reset/Menu indicators (shown based on input scheme)
  - Star target display (optimal turn count)
- `src/scene/pause_scene.h / .c` — Pause overlay (pushed on PAUSE action). Resume, Undo, Reset, Settings, Quit to Overworld. Semi-transparent backdrop.

**Verification:** HUD elements visible during play, pause overlay works, resume returns to gameplay.

---

## Step 9 — Level Results and Star Rating

End-of-level flow with star display and progression.

**Files:**

- `src/scene/results_scene.h / .c` — Shown after level win:
  - Display stars earned (1 for completion, 2 if turns ≤ optimal)
  - Show turn count vs. optimal
  - Options: Next Level, Retry, Quit to Overworld
- Update `src/data/save_data.c` — Write level completion, best turns, stars to save slot on win. Auto-save.

**Verification:** Win a level → results screen shows correct stars → save file updated → "Next Level" loads next level.

---

## Step 10 — Backdrop Render-to-Texture System

Offscreen FBO rendering pipeline for pre-rendering static geometry to textures. This is the foundation for both puzzle backdrops and overworld backdrops. See [02 -- Visual Style](02-visual-style.md) §9 and [08 -- Engine Architecture](08-engine-architecture.md) §3.3.6.

**Files:**

- `src/render/backdrop.h / .c` — Backdrop render-to-texture system:
  - `Backdrop` struct: FBO handle, color texture handle, pixel dimensions, ortho projection matrix.
  - `backdrop_create(Backdrop*, int width, int height)` — Allocate FBO + color texture at exact pixel dimensions.
  - `backdrop_begin_render(Backdrop*, const mat4 ortho_proj)` — Bind FBO, set viewport to FBO dimensions, set projection. Stores the ortho matrix for pixel-alignment verification.
  - `backdrop_end_render(Backdrop*)` — Unbind FBO, restore previous viewport.
  - `backdrop_draw(const Backdrop*, GLuint shader)` — Draw the backdrop as a single textured quad at Z-behind all live geometry, using the stored ortho projection. Uses `GL_NEAREST` sampling to prevent sub-pixel blur.
  - `backdrop_resize(Backdrop*, int new_width, int new_height)` — Reallocate FBO at new dimensions. Called on viewport resize.
  - `backdrop_destroy(Backdrop*)` — Release FBO + texture.
- Update `src/render/README.md` — Add backdrop entry.
- Integration into puzzle scene (render the backdrop mesh split — decorative geometry only — into the FBO at level load, draw it each frame behind gameplay layer).

**Pixel alignment contract:**

- FBO dimensions must exactly match the target viewport pixel dimensions.
- The ortho projection used for the FBO render must be bit-identical to the live gameplay projection.
- The backdrop quad uses `GL_NEAREST` filtering (1:1 texel-to-pixel).
- On viewport resize, the backdrop is re-rendered at the new dimensions.

**Verification:** Pre-rendered backdrop is visually indistinguishable from live rendering. Toggle backdrop on/off at runtime (debug key) to confirm pixel-identical output. No seams visible at the boundary between backdrop and live gameplay geometry.

---

## Step 11 — Overworld Scene

Per-biome level-selection diorama with graph navigation. The overworld diorama is pre-rendered to a backdrop texture (using the system from Step 10), with live overlay elements composited on top.

**Files:**

- `src/scene/overworld_scene.h / .c` — Implements `State` vtable:
  - Load overworld graph definition (YAML) for current biome
  - Generate biome diorama mesh + all LOD mini-diorama meshes
  - Pre-render entire overworld to backdrop texture via `backdrop_*()` API
  - Draw backdrop each frame, then render live overlay elements on top:
    - Theseus player token (animated along paths)
    - Node state indicators (flags, torches, completion glow)
    - Optional idle decorative animations (birds, gears, water)
    - Star gate visual effects
  - Cardinal direction navigation between nodes
  - Enter node → push zoom transition → puzzle scene
  - Back → return to biome select or title
  - Star gate nodes block until threshold met
  - Re-render backdrop on secret node reveal or viewport resize
- `src/data/overworld_data.h / .c` — Load overworld YAML (nodes, edges, positions, star gates)
- `assets/overworld/stone_labyrinth.yml` — First biome overworld graph
- `assets/overworld/dark_forest.yml` — Second biome overworld graph

**Verification:** Navigate between level nodes, enter levels, see locked star gates. Backdrop rendering matches live rendering (toggle with debug key). Live overlay elements (Theseus token, state indicators) render correctly on top of the static image.

---

## Step 12 — LOD Mesh Generation

Simplified diorama meshes for overworld level nodes. These are baked into the overworld backdrop texture.

**Files:**

- `src/render/lod_gen.h / .c` — Generates simplified mini-dioramas from level data:
  - Flat checkerboard floor (no paving detail)
  - Wall silhouettes (no mortar/block detail)
  - Omit decorations, lanterns
  - Diorama platform silhouette
- Update `src/scene/overworld_scene.c` — Include LOD meshes in the overworld backdrop render pass. Node state indicators (completion flags, star overlays) are rendered as live overlay elements, not baked into the backdrop.

**Verification:** Overworld backdrop shows recognizable mini-dioramas at each node. Completed levels have live overlay visual indicators.

---

## Step 13 — Zoom Transitions

Seamless zoom between overworld and puzzle views. During transitions, the backdrop system is **bypassed** and all geometry renders live so that camera interpolation and LOD crossfade work seamlessly.

**Files:**

- `src/scene/zoom_transition.h / .c` — Transition state pushed between overworld and puzzle:
  - On start: flag backdrop system as bypassed, switch to live rendering of all geometry (both overworld terrain + LOD meshes and the incoming/outgoing puzzle mesh)
  - Zoom camera from overworld view → close-up on selected node
  - Crossfade LOD mesh → full-detail diorama mesh
  - Reverse on exit (puzzle → overworld)
  - Duration ~0.8–1.2 seconds
  - On completion: re-render backdrop at final camera position, resume steady-state backdrop compositing
  - Auto-progression (puzzle → puzzle): zoom out to mid-level, pan across overworld, zoom into next node — all live rendering during the move
- Update `src/render/camera.c` — Smooth camera interpolation support

**Verification:** Entering/exiting a level has smooth zoom animation with LOD crossfade. No pop-in or jarring cuts. Transition from live rendering back to backdrop compositing is seamless (no visible frame where the switch happens).

---

## Step 14 — Audio System

Sound effects, music, and ambient audio.

**Files:**

- `src/audio/audio_manager.h / .c` — SDL3 audio backend:
  - Music playback (OGG Vorbis) with crossfade on biome transitions
  - SFX playback (WAV) — fire-and-forget, multiple simultaneous
  - Ambient loops (OGG) — per-biome, crossfade
  - Volume control (reads from settings)
- `src/audio/audio_events.h` — Named audio event enum (AUDIO_SFX_FOOTSTEP_LIGHT, AUDIO_SFX_MINOTAUR_STEP, AUDIO_SFX_WIN_CHIME, etc.)
- Integration points:
  - Puzzle scene: footsteps on move, environmental triggers, win/loss stingers
  - Title/menu: UI navigation clicks, confirm/back sounds
  - Overworld: ambient biome audio, node enter sound
- `assets/audio/` — Placeholder audio files (can be silent stubs initially)

**Verification:** Footstep sounds play on move, UI has click feedback, music plays on title screen, volume settings work.

---

## Step 15 — Touch Input Adapter (iOS)

On-screen controls for iOS/iPadOS (portrait mode).

**Files:**

- `src/input/touch_adapter.h / .c` — Touch → semantic action mapping:
  - Virtual D-pad (bottom-left)
  - Action buttons (bottom-right): Wait, Undo, Reset, Pause
  - Swipe gestures as alternative to D-pad
- `src/render/touch_overlay.h / .c` — Render on-screen control graphics
- Update `src/render/renderer.c` — Square viewport at screen top (portrait), controls area below
- Update `src/input/input_manager.c` — Register touch adapter

**Verification:** Touch controls respond correctly on iOS simulator or device. Viewport correctly sized as square with controls below.

---

## Step 16 — Apple TV Remote Adapter

Siri Remote and MFi gamepad support for tvOS.

**Files:**

- `src/input/remote_adapter.h / .c` — Apple TV Siri Remote → semantic action:
  - Clickpad directions → movement/navigation
  - Center click → CONFIRM / WAIT
  - Menu → BACK / PAUSE
  - Play/Pause → WAIT
- Focus engine integration (highlight management for menus)
- Update `src/input/input_manager.c` — Register remote adapter on tvOS

**Verification:** Full game playable with Siri Remote. Menu navigation works with focus system.

---

## Step 17 — Level Content (Full Biome Set)

Author all level JSON files and overworld graphs for every biome.

**Content:**

- Ship of Theseus (tutorial): 3 levels introducing movement, Minotaur, exit
- Stone Labyrinth: ~10 levels (walls only, core mechanic mastery)
- Dark Forest: ~10 levels (introduces spike traps)
- Remaining ~10 biomes: ~10 levels each, progressively introducing features
- Overworld YAML for each biome (node graph, star gates, secrets)
- Biome config JSON for each (palette, procgen params)

**Verification:** Complete playthrough from tutorial to final biome. Difficulty curve feels right. Star gates have reasonable thresholds. Secret levels discoverable.

---

## Step 18 — Audio Content

Replace placeholder audio with final assets.

**Content:**

- Title theme music
- Per-biome music tracks (12+)
- Per-biome ambient loops
- SFX: footsteps (light + heavy), spike activation, door unlock, teleporter, conveyor, crumble, pressure plate click, win chime, loss stinger, undo rewind, reset scatter, UI clicks

**Verification:** Audio enhances gameplay feel. Crossfades smooth. Volume levels balanced.

---

## Step 19 — Font and Visual Assets

Replace placeholder font and add any remaining visual assets.

**Content:**

- Final thematic font (theseus.ttf replacement)
- Any static textures needed (unlikely given procedural approach)
- App icons for each platform

**Verification:** Font reads well at all 5 sizes. Icons display correctly on all target platforms.

---

## Step 20 — Platform Builds and Testing

Build, test, and fix per-platform issues.

**Tasks:**

- Windows build (MSVC + MinGW)
- macOS universal build (arm64 + x86_64)
- Linux build (GCC)
- Steam Deck testing (1280×800, gamepad-only)
- iOS/iPadOS build (Xcode, portrait, touch)
- tvOS build (Xcode, Siri Remote)
- Performance profiling — 60 FPS on all targets
- Save path verification per platform
- Gamepad hot-plug testing
- Resolution switching / fullscreen toggle

**Verification:** Game runs at 60 FPS with correct input, rendering, and save behavior on every target platform.

---

## Step 21 — Polish and Ship

Final pass before release.

**Tasks:**

- Difficulty tuning (adjust optimal turn counts, star gate thresholds)
- Animation timing polish
- Visual consistency pass (biome palettes, decoration density)
- Accessibility review (text contrast, colorblind considerations)
- Memory leak audit (Valgrind / ASan)
- Crash testing (rapid input, edge cases, save corruption recovery)
- Steam integration (achievements if applicable, store page)
- App Store submissions (iOS, tvOS)
- Localization hooks verified (even if English-only at launch)
