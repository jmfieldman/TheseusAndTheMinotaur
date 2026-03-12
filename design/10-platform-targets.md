# 10 -- Platform Targets

## 1. Launch Platforms (Steam)

### 1.1 Windows

- **Target:** Windows 10+
- **Graphics:** OpenGL 3.3 Core (widely supported)
- **Input:** Keyboard + mouse, Xbox/generic gamepad (via SDL3 gamepad API)
- **Distribution:** Steam
- **Build:** MSVC or MinGW, 64-bit

### 1.2 macOS

- **Target:** macOS 12+ (Monterey)
- **Graphics:** OpenGL 4.1 (last version Apple supported; deprecated but
  functional)
- **Input:** Keyboard, PS/Xbox gamepad (via SDL3)
- **Distribution:** Steam
- **Build:** clang, universal binary (x86_64 + arm64)

> **Note:** OpenGL is deprecated on macOS but remains shipped and functional.
> No Metal backend is planned. The game's modest rendering requirements are
> well within OpenGL's capabilities on all Apple platforms.

### 1.3 Steam Deck

- **Target:** SteamOS (Linux-based)
- **Graphics:** OpenGL 3.3 Core (via Mesa)
- **Input:** Built-in gamepad + touchscreen
- **Distribution:** Steam (same build as Linux desktop)
- **Notes:**
  - Must test at 1280x800 (native Steam Deck resolution).
  - Touch overlay should activate when touchscreen is used.
  - Verify gamepad mappings via SDL3's Steam Deck support.

### 1.4 Linux (Desktop)

- **Target:** Ubuntu 22.04+ / SteamOS
- **Graphics:** OpenGL 3.3 Core
- **Input:** Keyboard, gamepad
- **Distribution:** Steam
- **Build:** gcc or clang, 64-bit

## 2. Fast-Follow Platforms

### 2.1 Apple TV (tvOS)

- **Target:** tvOS 16+
- **Graphics:** OpenGL ES 3.0
- **Input:** Siri Remote (see [06 -- Input](06-input.md) §3.4), MFi gamepad
- **Distribution:** App Store
- **Build:** Xcode (CMake-generated), arm64
- **Constraints:**
  - Siri Remote has very limited buttons -- Undo/Reset must be in pause menu.
  - No touch overlay (TV is not touch-interactive).
  - Must support Apple TV focus engine for menus, or implement custom focus
    navigation.

### 2.2 iOS / iPadOS

- **Target:** iOS 16+ / iPadOS 16+
- **Graphics:** OpenGL ES 3.0
- **Input:** Touch (primary), MFi gamepad (optional)
- **Distribution:** App Store
- **Build:** Xcode (CMake-generated), arm64
- **Orientation:** Supports both **portrait and landscape**.
- **Layout:** The game renders into a **square viewport**; the remaining screen
  area is used for touch controls (see [06 -- Input](06-input.md) §5).
  - Portrait: game square at top, controls below.
  - Landscape: game square on left, controls on right.
- **Constraints:**
  - Must handle multiple screen sizes (iPhone SE through iPad Pro 12.9").
  - Minimum control zone sizes must be enforced (especially on near-square
    iPads where the controls strip is narrow).
  - When a keyboard or gamepad is connected, touch controls hide and the game
    viewport expands to full screen.
  - Must handle interruptions (phone calls, notifications, backgrounding).
  - Auto-save on app backgrounding.

## 3. Platform Abstraction

The engine should isolate platform-specific code behind abstraction layers:

| Layer           | Platform-Specific Code                       |
|-----------------|----------------------------------------------|
| Windowing       | SDL3 handles this (cross-platform)           |
| Graphics API    | OpenGL 3.3 Core (desktop) / ES 3.0 (mobile)  |
| Input           | Input adapter per platform (see 06-input.md) |
| File I/O        | Save file paths differ per platform          |
| Audio           | SDL3 handles this (cross-platform)           |
| App Lifecycle   | iOS/tvOS need backgrounding/foregrounding    |

### 3.1 Save File Locations

| Platform    | Save Path                                      |
|-------------|-------------------------------------------------|
| Windows     | `%APPDATA%/TheseusAndTheMinotaur/saves/`        |
| macOS       | `~/Library/Application Support/TheseusAndTheMinotaur/saves/` |
| Linux       | `~/.local/share/TheseusAndTheMinotaur/saves/`   |
| iOS/iPadOS  | App sandbox Documents directory                  |
| tvOS        | iCloud KV store or App sandbox                   |

> **Open question:** Steam Cloud for save sync on desktop? iCloud for
> Apple platforms?

## 4. Performance Targets

| Platform         | Target FPS | Resolution            |
|------------------|------------|-----------------------|
| Desktop (Win/Mac/Linux) | 60 | Native (up to 4K)     |
| Steam Deck       | 60         | 1280x800              |
| Apple TV         | 60         | 1080p / 4K            |
| iPad Pro         | 60         | Native (up to 2732x2048) |
| iPhone           | 60         | Native                |

The game is not graphically demanding (low-poly, static dioramas, minimal
post-processing), so 60 FPS should be easily achievable on all targets.

## 5. Certification & Compliance

| Platform | Key Requirements                                    |
|----------|-----------------------------------------------------|
| Steam    | Steamworks integration, achievements (optional)     |
| App Store| App Review guidelines, privacy manifest, age rating |
| tvOS     | Top Shelf image, Siri Remote as primary input       |
