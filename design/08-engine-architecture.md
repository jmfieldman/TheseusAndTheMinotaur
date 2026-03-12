# 08 -- Engine Architecture

## 1. Technology Stack

| Component       | Technology          |
|-----------------|---------------------|
| Language        | **C** (C11 or later)|
| Build System    | **CMake**           |
| Windowing / OS  | SDL3                |
| Rendering       | OpenGL (3.3 Core on desktop, ES 3.0 on mobile) |
| Audio           | SDL3 Audio          |
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
1. Set viewport (full screen on desktop, square sub-region on iOS)
2. Clear framebuffer
3. Set camera / projection uniforms
4. Render diorama (static voxel geometry) -- could be a single pre-built VBO
5. Render actors (Theseus, Minotaur) -- animated meshes
6. Render environmental features (animated/stateful geometry)
7. Post-processing pass (vignette, etc.)
8. Reset viewport to full screen
9. Render overlays (HUD, touch controls, UI)
```

#### 3.3.3 Voxel Rendering

Each diorama is a collection of voxel meshes. Options for rendering:

- **Pre-baked mesh:** The entire diorama is a single static mesh loaded from
  an asset file. Simplest and fastest.
- **Instanced rendering:** Voxels are instances of a few base shapes (cube,
  beveled cube, rounded cube) with per-instance transforms and colors.
- **Hybrid:** Static diorama as pre-baked mesh; dynamic elements (actors,
  environmental features) as separate instanced meshes.

> **TBD:** Which approach depends on asset pipeline decisions (see
> [09 -- Content Pipeline](09-content-pipeline.md)).

#### 3.3.4 Shader Requirements

Minimal shader set:

| Shader         | Purpose                                     |
|----------------|---------------------------------------------|
| Diorama        | Static geometry with baked AO, vertex color |
| Actor          | Animated mesh with flat shading + rim light |
| UI / Overlay   | Screen-space 2D quads, text rendering       |
| Post-process   | Full-screen vignette, optional bloom        |

### 3.4 Animation System

- **Tween-based** animation for actor movement (position lerp over N frames).
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

## 4. Resource Management

- Assets loaded at scene transitions (not during gameplay).
- Each biome's assets (meshes, textures, audio) are a loadable bundle.
- Simple scope-based lifetime (load on biome enter, release on biome exit).
- C structs with explicit init/destroy functions (no RAII).

## 5. Threading Model

- **Single-threaded** for game logic and rendering.
- Audio playback on a separate thread (handled by SDL audio callbacks).
- Asset loading may use a background thread with a loading screen.

## 6. Build System

**CMake** is the build system. Must support:

- Windows (MSVC or MinGW, 64-bit)
- macOS (clang, universal binary for x86_64 + arm64)
- Linux (gcc/clang, 64-bit)
- iOS / iPadOS (Xcode project generation via `cmake -G Xcode`)
- tvOS (Xcode project generation via `cmake -G Xcode`)

### 6.1 Project Structure (Preliminary)

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
