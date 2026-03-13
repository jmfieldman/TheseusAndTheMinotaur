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
- Accept puzzle input context actions (MOVE_*, WAIT, UNDO, RESET, PAUSE)
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

Procedural actor models with proper animations, including per-cause death animations with undo reversal.

**Files:**
- `src/render/actor_render.h / .c` — Generates Theseus and Minotaur voxel models. Theseus: beveled golden cube (~40-50% tile), composed of individual boxes that can separate for death animations. Minotaur: larger dark red cube (~75% tile) with white horn nubs and face detail. Renders at interpolated animation position each frame.
- Update `src/engine/anim_queue.c` with actor-specific animations:
  - Theseus: parabolic hop (no rotation), lean into jump, squash on landing
  - Minotaur: 90° roll per step (horns retract before roll, extend after)
  - Ground shake on heavy landing (Minotaur)
  - Win animation (Theseus spins/glows at exit)
- `src/render/death_anim.h / .c` — Per-cause death animations (see design doc §02 §7.7). Each animation decomposes Theseus into individual voxel boxes and animates them differently. All animations are reversible for undo:
  - **Minotaur squish** (§7.7.1): Minotaur rolls onto Theseus, voxels flatten and scatter outward from impact. Extra ground shake.
  - **Walk into Minotaur** (§7.7.2): Theseus hop cuts short mid-arc, voxels shatter on impact and scatter backward. Minotaur recoil pulse.
  - **Spike impale** (§7.7.3): Spikes launch through tile, Theseus voxels pop upward and scatter. Gold voxels remain impaled on spike tips.
  - **Medusa petrification** (§7.7.4): Grey color wave sweeps across voxels, Theseus freezes mid-motion, then cracks and crumbles into rubble.
  - **Pit fall** (§7.7.5): Voxels drop coherently into hole with tumble rotation, shrink toward vanishing point. Lava variant: voxels glow orange/red and dissolve.
- Each death animation stores enough state to play in reverse when undo is triggered (voxel positions, colors, timing keyframes).

**Verification:** Trigger each death type and verify: (1) animation plays correctly, (2) undo from death reverses the animation smoothly (voxels reconstitute), (3) Theseus is playable again after undo. Also verify movement animations (hop, roll) play correctly with input buffering.

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

## Step 10 — Overworld Scene

Per-biome level-selection diorama with graph navigation.

**Files:**
- `src/scene/overworld_scene.h / .c` — Implements `State` vtable:
  - Load overworld graph definition (YAML) for current biome
  - Render biome diorama with level nodes as mini-dioramas
  - Theseus marker on current node
  - Cardinal direction navigation between nodes
  - Enter node → push puzzle scene (or transition)
  - Back → return to biome select or title
  - Star gate nodes block until threshold met
  - Auto-progression: after win, animate to next unbeaten node
- `src/data/overworld_data.h / .c` — Load overworld YAML (nodes, edges, positions, star gates)
- `assets/overworld/stone_labyrinth.yml` — First biome overworld graph
- `assets/overworld/dark_forest.yml` — Second biome overworld graph

**Verification:** Navigate between level nodes, enter levels, see locked star gates, auto-progress after winning.

---

## Step 11 — LOD Mesh Generation

Simplified diorama meshes for overworld level nodes.

**Files:**
- `src/render/lod_gen.h / .c` — Generates simplified mini-dioramas from level data:
  - Flat checkerboard floor (no paving detail)
  - Wall silhouettes (no mortar/block detail)
  - Omit decorations, lanterns, back wall details
  - Completion indicator (checkmark/star overlay)
- Update `src/scene/overworld_scene.c` — Render LOD meshes at each node position

**Verification:** Overworld shows recognizable mini-dioramas at each node. Completed levels have visual indicators.

---

## Step 12 — Zoom Transitions

Seamless zoom between overworld and puzzle views.

**Files:**
- `src/scene/zoom_transition.h / .c` — Transition state pushed between overworld and puzzle:
  - Zoom camera from overworld view → close-up on selected node
  - Crossfade LOD mesh → full-detail diorama mesh
  - Reverse on exit (puzzle → overworld)
  - Duration ~0.8–1.2 seconds
- Update `src/render/camera.c` — Smooth camera interpolation support

**Verification:** Entering/exiting a level has smooth zoom animation with LOD crossfade. No pop-in or jarring cuts.

---

## Step 13 — Audio System

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

## Step 14 — Touch Input Adapter (iOS)

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

## Step 15 — Apple TV Remote Adapter

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

## Step 16 — Level Content (Full Biome Set)

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

## Step 17 — Audio Content

Replace placeholder audio with final assets.

**Content:**
- Title theme music
- Per-biome music tracks (12+)
- Per-biome ambient loops
- SFX: footsteps (light + heavy), spike activation, door unlock, teleporter, conveyor, crumble, pressure plate click, win chime, loss stinger, undo rewind, reset scatter, UI clicks

**Verification:** Audio enhances gameplay feel. Crossfades smooth. Volume levels balanced.

---

## Step 18 — Font and Visual Assets

Replace placeholder font and add any remaining visual assets.

**Content:**
- Final thematic font (theseus.ttf replacement)
- Any static textures needed (unlikely given procedural approach)
- App icons for each platform

**Verification:** Font reads well at all 5 sizes. Icons display correctly on all target platforms.

---

## Step 19 — Platform Builds and Testing

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

## Step 20 — Polish and Ship

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
