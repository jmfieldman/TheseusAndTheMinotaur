# 08 -- Engine Architecture

## 1. Technology Stack

| Component       | Technology          |
|-----------------|---------------------|
| Language        | **C** (C11 or later)|
| Build System    | **CMake**           |
| Windowing / OS  | SDL3                |
| Rendering       | OpenGL (3.3 Core on desktop, ES 3.0 on mobile) |
| Audio           | SDL3 Audio          |
| Text Rendering  | SDL_ttf (TrueType)  |
| JSON Parsing    | cJSON (MIT)         |
| YAML Parsing    | libyaml (MIT)       |
| Input           | SDL3 Events         |

### 1.1 Language Choice: C

Pure C was chosen for:

- **Simplicity** -- the game logic is not complex enough to benefit from C++
  abstractions.
- **Maximum portability** -- C compiles everywhere with minimal toolchain fuss.
- **Predictable behavior** -- no hidden constructors, destructors, or vtable
  overhead.
- **SDL3 native** -- SDL3 is a C library; no wrapper friction.

### 1.2 Graphics: OpenGL Only

The engine uses **OpenGL exclusively** (no Metal backend). Rationale:

- OpenGL 3.3 Core is supported on Windows, macOS, Linux, and Steam Deck.
- OpenGL ES 3.0 is supported on iOS, iPadOS, and tvOS.
- The game's rendering needs are modest (low-poly, baked lighting, minimal
  post-processing) -- OpenGL is more than sufficient.
- While OpenGL is deprecated on Apple platforms, it remains functional and
  Apple continues to ship it. A Metal port can be considered in the future if
  Apple removes OpenGL support, but this is not expected in the near term.
- Avoiding a split rendering backend keeps the codebase simple and
  maintainable.

## 2. High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      Application                        │
│  ┌─────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐  │
│  │  Input   │  │  Game    │  │ Renderer │  │  Audio  │  │
│  │ Manager  │  │  Logic   │  │          │  │ Manager │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬────┘  │
│       │             │             │              │       │
│       ▼             ▼             ▼              ▼       │
│  ┌──────────────────────────────────────────────────┐   │
│  │              Scene / State Manager               │   │
│  │  (Title, SaveSelect, Overworld, Puzzle, Pause)   │   │
│  └──────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│  ┌──────────────────────────────────────────────────┐   │
│  │                   SDL3 Layer                     │   │
│  │  (Window, GL Context, Events, Audio Device)      │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## 3. Core Systems

### 3.1 Game Loop

Standard fixed-timestep game loop:

```c
while (running) {
    process_input();       // SDL events -> Input Manager -> semantic actions
    update(delta_time);    // Advance animations, resolve queued actions
    render();              // Draw current scene
    present();             // SDL_GL_SwapWindow
}
```

- **Fixed logic timestep** (e.g. 60 ticks/sec) with interpolated rendering for
  smooth animation.
- Since the game is turn-based, most "update" work is animation/tween playback
  rather than physics.

### 3.2 Scene / State Manager

A stack-based state machine manages the active game screen:

| State         | Description                                    |
|---------------|------------------------------------------------|
| TitleScreen   | Main menu                                       |
| SaveSelect    | Save slot picker                                |
| Overworld     | Biome diorama navigation (contains all mini-diorama LOD meshes) |
| Puzzle        | Active puzzle level (zoomed into one diorama)   |
| ZoomTransition| Animating between Overworld and Puzzle (or Puzzle to Puzzle) |
| PauseOverlay  | Overlay on top of Puzzle state (pushed on stack) |
| Settings      | Settings screen (can be pushed from multiple states) |

States can push/pop (e.g. Puzzle pushes PauseOverlay), and the state at the
top of the stack receives input and renders on top.

The **Overworld** and **Puzzle** states share the same scene geometry --
the overworld contains all puzzle dioramas as LOD meshes. The
**ZoomTransition** state manages the camera interpolation and LOD↔full-detail
mesh crossfade between them. See §3.3.5 for the zoom camera system.

### 3.3 Renderer

#### 3.3.1 Projection

- **Orthographic projection** for all gameplay views.
- Camera parameters: position, look-at, up vector, ortho bounds.
- Ortho bounds adjust based on grid size and screen aspect ratio.
- On iOS, the game renders into a **square viewport** (see
  [06 -- Input](06-input.md) §5), so the renderer must support variable
  viewport dimensions independent of the window/screen size.

#### 3.3.2 Rendering Pipeline

```
 1. Shadow pass: render scene from each dynamic light's perspective into
    shadow map(s) (actors, walls, dynamic features)
 2. Set viewport (full screen on desktop, square sub-region on iOS)
 3. Clear framebuffer
 4. Set camera / projection uniforms
 5. Render diorama (static voxel geometry with baked AO + shadows)
 6. Render environmental features (animated/stateful geometry)
 7. Render actors (Theseus, Minotaur) with real-time shadow sampling
 8. Render exit tile god-light volumetric effect
 9. Apply dynamic lighting from environmental light sources
10. Post-processing pass (vignette, bloom on emissive elements)
11. Reset viewport to full screen
12. Render overlays (HUD, touch controls, UI)
```

#### 3.3.3 Voxel Rendering (Procedural)

All level dioramas are **procedurally generated at runtime** from logical level
data + biome configuration (see [09 -- Content Pipeline](09-content-pipeline.md)
§3). The engine builds GPU-ready geometry at two detail levels:

**Full-detail mesh** (generated when entering a puzzle):
- **Static diorama mesh:** Includes floor tiles (checkerboard with paving-
  stone blocks), walls (biome-styled voxel compositions with mortar gaps),
  back wall (tall thematic backdrop), entrance/exit doors, impassable tile
  fill, all decoration layers (floor scatter, wall moss/cracks, wall-top
  crumble), edge borders, lantern pillars, and exit door god-light geometry.
  Packed into a single VBO with vertex colors.
- **Dynamic elements:** Actors (Theseus, Minotaur), entrance door lock
  mechanism, and stateful environmental features (spike traps, pressure
  plates, etc.) are rendered as separate meshes that update per frame.

**LOD mesh** (generated for all puzzles when a biome loads):
- Simplified version of the diorama for overworld display. Includes floor
  tiles (flat checkerboard, no individual paving blocks), wall silhouettes
  (simplified block shapes), back wall silhouette, entrance/exit door
  openings, diorama platform, and lantern pillars. Decoration layers are
  omitted. See [09 -- Content Pipeline](09-content-pipeline.md) §3.9.
- All ~10 LOD meshes for a biome are held in VRAM simultaneously.
- Each LOD mesh is small enough (low voxel count, no decorations) that
  the combined memory footprint is manageable.

- **Instanced rendering** may be used for repeated voxel shapes within the
  static mesh (e.g. floor voxels, uniform wall blocks) to reduce draw calls.

The procedural generator uses a **seeded RNG** (seed = level ID + tile
coordinate) to ensure deterministic, reproducible output across runs and
platforms.

#### 3.3.5 Zoom Camera System

The zoom transition between overworld and puzzle views is implemented as a
smooth interpolation of the **orthographic projection bounds**:

- **Overworld framing:** Ortho bounds encompass the entire biome diorama
  (all mini-dioramas visible).
- **Puzzle framing:** Ortho bounds tightened to frame a single puzzle diorama
  with appropriate padding.
- **Zoom animation:** Smoothly interpolate ortho bounds (position + scale)
  using an easing curve (e.g. ease-in-out cubic). Duration ~0.8--1.2s.
- **LOD crossfade:** During zoom-in, once the target diorama exceeds a
  screen-space size threshold, the LOD mesh fades out and the full-detail
  mesh fades in (alpha crossfade over ~0.2--0.3s). Reverse on zoom-out.
- **Auto-progression (puzzle → puzzle):** Zoom out to a mid-level view
  (partial overworld), pan to the next puzzle node, zoom in. The zoom-out
  and pan can overlap for a fluid camera move.

#### 3.3.4 Shader Requirements

| Shader         | Purpose                                              |
|----------------|------------------------------------------------------|
| Shadow depth   | Render depth from light POV for shadow mapping       |
| Diorama        | Static geometry with baked AO, vertex color, shadow sampling |
| Actor          | Animated mesh with flat shading, rim light, shadow cast/receive |
| Dynamic light  | Additive per-light pass with shadow map sampling     |
| God-light      | Exit tile volumetric cone effect                     |
| UI / Overlay   | Screen-space 2D quads, SDL_ttf text                  |
| Post-process   | Full-screen vignette, bloom on emissive elements     |

### 3.4 Animation System

- **Tween-based** animation for actor movement.
  - **Theseus:** Position follows a parabolic arc (hop); no rotation. Subtle
    vertical squash on landing. Entrance animation (hop in from outside grid
    through entrance door).
  - **Minotaur:** Position follows a slight arc; rotation is 90° around the
    leading bottom edge (roll). Horn retract/extend tweens at roll start/end.
    Localized ground-shake effect on landing (camera micro-shake + optional
    impact visual). Drop-in animation (fall from above onto starting tile).
- **Event-driven** animation for environmental features. Each feature records
  typed animation events during turn resolution; the animation queue replays
  them with per-type visual effects and timing (see §3.4.2).
- **Level lifecycle animations:**
  - **Level start:** Minotaur drop-in → Theseus entrance hop → door lock.
  - **Level reset:** Actors lift off-screen → environment reset → Minotaur
    drop-in → Theseus entrance → door lock.
  - **Level win:** Theseus exits through exit door onto virtual tile.
  - **Zoom transitions:** Managed by the ZoomTransition state (see §3.2).
- Animation playback is decoupled from game logic -- logic resolves instantly,
  then the renderer plays back the visual transition.

#### 3.4.1 Non-Blocking Animation Queue

The animation system plays back each turn as a **five-phase sequence**:

1. **Theseus phase** — Theseus move animation (hop, ice slide, teleport, or
   push). The animation type is determined by the `AnimEvent` recorded during
   turn resolution. Multi-part moves use sub-phases (e.g. ice slide: hop to
   first tile → constant-velocity slide through remaining waypoints).
2. **Theseus effects phase** — On-leave effects that fire after Theseus exits
   a tile (crumbling floor collapse, locking gate slam, pressure plate toggle).
   Events play sequentially with type-specific durations.
3. **Environment phase** — Environment events play sequentially (spike
   extend/retract, auto-turnstile rotation, platform movement, conveyor push).
   Each event has its own duration. If no events exist, a minimal 0.10s pause
   preserves turn rhythm.
4. **Minotaur step 1** — First Minotaur move animation.
5. **Minotaur step 2** — Second Minotaur move animation (if applicable).

Key behavior:

- When the player commits an action, the game logic resolves the full turn
  immediately (Theseus + environment + Minotaur) and records the resulting
  animation events into the `TurnRecord`.
- Animations always play out fully -- they are never fast-forwarded or skipped.
- The **input buffer window** opens during the Minotaur's last step animation.
  Only fresh key-down events are accepted (held keys ignored); last press wins.
- If the resolved state is a **death**, the animation queue plays through to
  the death moment, then blocks further gameplay input. (See
  [01 -- Core Mechanics](01-core-mechanics.md) §10.)

#### 3.4.2 AnimEvent System

Game logic resolves turns instantly. During resolution, features record **typed
animation events** (`AnimEvent`) into a flat array on the `TurnRecord`. This
decouples game logic from animation -- features describe *what happened*; the
animation queue decides *how it looks*.

**Recording mechanism:** The `Grid` struct holds a transient `active_record`
pointer (set at the start of `turn_resolve()`, cleared at the end). Features
push events from their existing vtable hooks (on_enter, on_leave, on_push,
on_environment_phase) via `turn_record_push_event()` -- no vtable signature
changes are needed. Maximum 32 events per turn.

**Event types and phases:**

| Phase | Event Type | Source Feature | Duration |
|-------|-----------|---------------|----------|
| Theseus | `THESEUS_HOP` | Normal move (turn.c) | 0.15s |
| Theseus | `THESEUS_ICE_SLIDE` | Ice tile (turn.c) | 0.15s hop + 0.06s/tile slide |
| Theseus | `THESEUS_TELEPORT` | Teleporter | 0.10s out + 0.10s in |
| Theseus | `BOX_SLIDE` + `THESEUS_PUSH_MOVE` | Groove box | 0.15s (concurrent) |
| Theseus | `TURNSTILE_ROTATE` | Manual turnstile | 0.20s |
| Theseus effect | `FLOOR_CRUMBLE` | Crumbling floor | 0.15s |
| Theseus effect | `GATE_LOCK` | Locking gate | 0.12s |
| Theseus effect | `PLATE_TOGGLE` | Pressure plate | 0.10s |
| Environment | `SPIKE_CHANGE` | Spike trap | 0.12s |
| Environment | `AUTO_TURNSTILE_ROTATE` | Auto-turnstile | 0.25s |
| Environment | `PLATFORM_MOVE` | Moving platform | 0.20s |
| Environment | `CONVEYOR_PUSH` | Conveyor | 0.15s |

Each event carries type-specific data (positions, waypoints, direction flags,
actor riding flags, etc.) as a tagged union. The animation queue reads these
events and sets up appropriate tweens for visual playback.

**Theseus sub-phases** for multi-part moves:
- **Ice slide:** (1) Parabolic hop to first ice tile, (2) constant-velocity
  linear slide through recorded waypoints (no hop during slide).
- **Teleport:** (1) Scale/fade out at source, (2) scale/fade in at destination.
- **Push:** Box slides to new position while Theseus steps into vacated tile
  (concurrent tweens).

#### 3.4.3 Reverse Undo Animation

Undo plays back the turn's animation in **reverse** at **2× speed** with a
visual "rewind" overlay, giving the feeling of rewinding a tape:

**Reverse phase order** (opposite of forward):

1. **Minotaur step 2** (reversed: after2 → after1)
2. **Minotaur step 1** (reversed: after1 → env start position)
3. **Environment phase** (events iterated in reverse order, from/to swapped)
4. **Theseus effects** (events iterated in reverse, progress runs 1→0)
5. **Theseus move** (reversed: to → from, including reverse ice slide and
   reverse teleport)

**Architecture:**

- The `TurnRecord` is stored alongside each `UndoSnapshot` (via
  `undo_store_turn_record()`) after `turn_resolve()` fills it.
- On undo, `anim_queue_start_reverse()` plays the record backward. The
  actual grid restore (`undo_pop()`) is **deferred** until the reverse
  animation completes.
- During reverse playback, a semi-transparent blue-tinted overlay with
  horizontal scan lines creates a "VHS rewind" visual effect.
- All durations are divided by `ANIM_REVERSE_SPEED` (2.0×) so undo feels
  snappy but still visually informative.
- If no `TurnRecord` is stored (e.g. first turn after session restore),
  undo falls back to instant grid restore.

### 3.5 Game Logic Module

Entirely separate from rendering:

- Pure functions that take board state + action and return new board state.
- No dependencies on SDL, OpenGL, or any platform API.
- Testable in isolation (headless unit tests).
- Manages: grid, walls, actor positions, environmental feature states, turn
  counter, undo stack.

```
┌──────────────────────────────────────────┐
│          Game Logic (pure C)             │
│  ┌─────────────┐  ┌──────────────────┐  │
│  │ Board State  │  │ Turn Resolution  │  │
│  │ • grid       │  │ • resolve_turn() │  │
│  │ • walls      │  │ • check_win()    │  │
│  │ • actors     │  │ • check_loss()   │  │
│  │ • features   │  │ • apply_undo()   │  │
│  │ • undo_stack │  │ • apply_reset()  │  │
│  └─────────────┘  └──────────────────┘  │
│                                          │
│  No SDL / OpenGL / platform dependencies │
│  Fully unit-testable                     │
└──────────────────────────────────────────┘
```

## 4. Localization

### 4.1 String Table

All player-facing text is driven by a **string table** (key → localized string
mapping). The engine loads strings from a data file at startup, keyed by locale.

- Format: JSON or YAML file per locale (e.g. `strings/en.json`,
  `strings/fr.json`).
- Keys are used throughout the codebase; the renderer looks up display text at
  draw time.
- **English only at launch.** The architecture supports additional locales with
  no code changes -- only new string files are needed.

### 4.2 Font Considerations

- A single font file **`assets/fonts/theseus.ttf`** is used for all text
  rendering. Multiple sizes are rendered from this TTF at runtime.
- SDL_ttf supports Unicode / TrueType, so CJK and other scripts are feasible
  if the shipped font covers the required glyphs.
- Right-to-left text is **not** planned for launch but should be considered if
  Arabic or Hebrew locales are added later.

## 5. Resource Management

- **Startup:** The engine decompresses `gamedata.tar.gz` into memory, parsing
  all level JSON, biome configs, overworld definitions, and localization
  strings into runtime data structures. Font (`assets/fonts/theseus.ttf`) is
  loaded via SDL_ttf.
- Assets loaded at scene transitions (not during gameplay).
- Since all geometry is procedurally generated, there are **no mesh files** to
  load. Biome "assets" are the parsed JSON configs + audio files.
- **Biome load:** All LOD meshes for the biome's ~10 puzzles are procedurally
  generated and uploaded to VRAM. Overworld diorama mesh is procedurally
  generated. Audio (music, ambient, SFX) is loaded from disk. This all
  happens during the biome transition loading screen.
- **Puzzle enter:** Full-detail mesh for the target puzzle is procedurally
  generated and uploaded. The previous puzzle's full-detail mesh (if any) is
  released. Only one full-detail puzzle mesh is in memory at a time.
- **Biome exit:** All LOD meshes + overworld mesh released. Audio unloaded.
- Simple scope-based lifetime (load on biome enter, release on biome exit).
- C structs with explicit init/destroy functions (no RAII).

## 6. Threading Model

- **Single-threaded** for game logic and rendering.
- Audio playback on a separate thread (handled by SDL audio callbacks).
- Asset loading may use a background thread with a loading screen.

## 7. Build System

**CMake** is the build system. Must support:

- Windows (MSVC or MinGW, 64-bit)
- macOS (clang, universal binary for x86_64 + arm64)
- Linux (gcc/clang, 64-bit)
- iOS / iPadOS (Xcode project generation via `cmake -G Xcode`)
- tvOS (Xcode project generation via `cmake -G Xcode`)

### 7.1 Project Structure (Preliminary)

```
/
├── CMakeLists.txt
├── src/
│   ├── main.c
│   ├── game/           # Pure game logic (no platform deps)
│   ├── render/         # OpenGL rendering
│   ├── input/          # Input manager + adapters
│   ├── audio/          # Audio manager
│   ├── scene/          # Scene/state management
│   ├── ui/             # HUD, menus, touch overlay
│   └── platform/       # Platform-specific code (save paths, lifecycle)
├── assets/             # Runtime assets (meshes, textures, audio, levels)
├── design/             # Design documents (this directory)
├── tests/              # Unit tests for game logic
└── tools/              # Build/asset pipeline scripts
```
