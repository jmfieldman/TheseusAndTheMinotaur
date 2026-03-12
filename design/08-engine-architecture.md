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
| Overworld     | Biome diorama navigation                        |
| Puzzle        | Active puzzle level                              |
| PauseOverlay  | Overlay on top of Puzzle state (pushed on stack) |
| Settings      | Settings screen (can be pushed from multiple states) |

States can push/pop (e.g. Puzzle pushes PauseOverlay), and the state at the
top of the stack receives input and renders on top.

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
§3). The engine builds GPU-ready geometry on level load:

- **Static diorama mesh:** Generated once on level load. Includes floor tiles
  (checkerboard), walls (biome-styled voxel compositions), impassable tile
  fill, decorations (floor scatter, wall moss/cracks, wall-top crumble), edge
  borders, and light source geometry. Packed into a single VBO with vertex
  colors.
- **Dynamic elements:** Actors (Theseus, Minotaur) and stateful environmental
  features (spike traps, pressure plates, etc.) are rendered as separate
  meshes that update per frame.
- **Instanced rendering** may be used for repeated voxel shapes within the
  static mesh (e.g. floor voxels, uniform wall blocks) to reduce draw calls.

The procedural generator uses a **seeded RNG** (seed = level ID + tile
coordinate) to ensure deterministic, reproducible output across runs and
platforms.

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
    vertical squash on landing.
  - **Minotaur:** Position follows a slight arc; rotation is 90° around the
    leading bottom edge (roll). Horn retract/extend tweens at roll start/end.
    Localized ground-shake effect on landing (camera micro-shake + optional
    impact visual).
- **State-driven** animation for environmental features (e.g. spike trap up/down
  transitions).
- Animation playback is decoupled from game logic -- logic resolves instantly,
  then the renderer plays back the visual transition.

#### 3.4.1 Non-Blocking Animation Queue

The animation system maintains a **queue of pending visual transitions**. Key
behavior:

- When the player commits an action, the game logic resolves the full turn
  immediately (Theseus + environment + Minotaur) and enqueues the resulting
  animation sequence.
- If the player inputs another action **while animations are still playing**:
  - All pending animations are **fast-forwarded** to their end states.
  - Actors and features snap to their resolved positions.
  - The new turn resolves and its animations are enqueued.
- If the resolved state is a **death**, the animation queue plays through to
  the death moment, then blocks further gameplay input. (See
  [01 -- Core Mechanics](01-core-mechanics.md) §10.)

This ensures the game feels **responsive at any pace** -- experienced players
can rapid-fire moves without waiting, while casual players see full animations.

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

- SDL_ttf supports Unicode / TrueType, so CJK and other scripts are feasible
  if the shipped font covers the required glyphs.
- Right-to-left text is **not** planned for launch but should be considered if
  Arabic or Hebrew locales are added later.

## 5. Resource Management

- Assets loaded at scene transitions (not during gameplay).
- Each biome's assets (meshes, textures, audio) are a loadable bundle.
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
