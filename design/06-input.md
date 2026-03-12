# 06 -- Input

## 1. Design Philosophy

The input system must be **completely abstracted** from the game logic and
presentation. The game engine receives **semantic actions** (e.g. "move north",
"undo"), never raw input events. This allows:

- Multiple input schemes to coexist and be hot-swapped.
- Platform-specific UI overlays (e.g. touch D-pad) to be added without
  modifying game logic.
- New input methods (e.g. Apple Remote) to be added as thin adapter layers.

### 1.1 Non-Blocking Input

The input system does **not** gate on animation state. The player can submit
their next action at any time, even while the previous turn's animations are
playing (see [01 -- Core Mechanics](01-core-mechanics.md) §10). The only
exception is when the game is in a **death state** -- further gameplay input
is blocked until the loss UI appears.

## 2. Semantic Action Set

### 2.1 Puzzle Actions

| Action       | Description                          |
|--------------|--------------------------------------|
| MOVE_NORTH   | Move Theseus up                      |
| MOVE_SOUTH   | Move Theseus down                    |
| MOVE_EAST    | Move Theseus right                   |
| MOVE_WEST    | Move Theseus left                    |
| WAIT         | Skip Theseus' turn                   |
| UNDO         | Undo last turn                       |
| RESET        | Reset level                          |
| PAUSE        | Open pause menu                      |

### 2.2 Menu / UI Actions

| Action        | Description                         |
|---------------|-------------------------------------|
| UI_UP         | Navigate menu up                    |
| UI_DOWN       | Navigate menu down                  |
| UI_LEFT       | Navigate menu left                  |
| UI_RIGHT      | Navigate menu right                 |
| UI_CONFIRM    | Select / confirm                    |
| UI_BACK       | Cancel / go back                    |

### 2.3 Overworld Actions

| Action         | Description                        |
|----------------|------------------------------------|
| OW_MOVE_NORTH  | Move token north                   |
| OW_MOVE_SOUTH  | Move token south                   |
| OW_MOVE_EAST   | Move token east                    |
| OW_MOVE_WEST   | Move token west                    |
| OW_ENTER       | Enter level at current node        |
| OW_BACK        | Return to previous screen          |

## 3. Input Adapters

Each input method is implemented as an **adapter** that translates raw
platform events into semantic actions.

### 3.1 Keyboard (Desktop)

| Key(s)            | Action          |
|-------------------|-----------------|
| W / Up Arrow      | MOVE_NORTH      |
| S / Down Arrow    | MOVE_SOUTH      |
| A / Left Arrow    | MOVE_WEST       |
| D / Right Arrow   | MOVE_EAST       |
| Space             | WAIT            |
| Z / Ctrl+Z        | UNDO            |
| R                 | RESET           |
| Escape            | PAUSE           |
| Enter             | UI_CONFIRM      |

> All keyboard bindings should be remappable in settings.

### 3.2 Gamepad (Desktop / Steam Deck)

| Input                | Action          |
|----------------------|-----------------|
| Left Stick / D-Pad   | Directional (digital threshold, not analog) |
| A / Cross            | UI_CONFIRM / WAIT (context-dependent) |
| B / Circle           | UI_BACK / UNDO (context-dependent)    |
| Y / Triangle         | RESET           |
| Start                | PAUSE           |

Gamepad analog sticks use **digital thresholding** (flick-to-move). Given the
turn-based nature of the game, the stick acts as a D-pad equivalent -- a flick
past the threshold registers one directional input. The stick must return to
center before another input is registered (no auto-repeat from holding).

### 3.3 Touch (iOS / iPadOS)

- **On-screen D-pad** for directional movement (positioned in the controls
  area, see §5).
- **On-screen buttons** for Wait, Undo, Reset, Pause.
- Touch controls are placed in the **dedicated controls area** and never
  overlap the game diorama (see §5 for layout).

### 3.4 Apple Remote (tvOS)

The Apple TV Siri Remote has a clickpad/touchpad, Menu button, and Play/Pause.

| Input               | Action            |
|----------------------|-------------------|
| Clickpad directions  | Directional       |
| Clickpad center      | UI_CONFIRM / WAIT |
| Menu button          | UI_BACK / PAUSE   |
| Play/Pause           | WAIT              |

> The limited button count on the remote means some actions (Undo, Reset) must
> be accessed through the pause menu rather than direct input.

## 4. Input Manager Architecture

```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│  Keyboard    │  │   Gamepad    │  │    Touch     │  ...
│  Adapter     │  │   Adapter    │  │   Adapter    │
└──────┬───────┘  └──────┬───────┘  └──────┬───────┘
       │                 │                 │
       ▼                 ▼                 ▼
    ┌──────────────────────────────────────────┐
    │           Input Manager                  │
    │  • Aggregates adapters                   │
    │  • Deduplicates / prioritizes            │
    │  • Emits semantic actions                │
    │  • Manages input context (puzzle/menu/ow)│
    │  • Does NOT block on animation state     │
    └──────────────────┬───────────────────────┘
                       │
                       ▼
                  Game Logic
```

### 4.1 Input Contexts

The Input Manager maintains the current **context** which determines how
semantic actions are routed:

- **Puzzle context** -- directional inputs become MOVE_*, confirm/back map to
  game actions.
- **Menu context** -- directional inputs become UI_*, confirm/back navigate
  menus.
- **Overworld context** -- directional inputs become OW_MOVE_*, confirm enters
  levels.

### 4.2 Multi-Input Handling

- Multiple adapters can be active simultaneously (e.g. keyboard + gamepad on
  desktop).
- If conflicting inputs arrive in the same frame, the most recent input wins.
- Hot-plugging is supported (gamepad connect/disconnect at runtime).

## 5. iOS / iPadOS Screen Layout

On iOS and iPadOS, the screen is divided into two distinct zones to ensure
controls never obscure the game view.

### 5.1 Layout Principle

The game engine always renders into a **square viewport**. The remaining screen
area is dedicated to UI controls (D-pad, action buttons, level info).

### 5.2 Portrait Orientation

```
┌──────────────────────┐
│                      │
│    Square Game       │
│    Viewport          │
│    (puzzle diorama)  │
│                      │
├──────────────────────┤
│                      │
│    Controls Area     │
│  ┌─────┐   ┌──────┐ │
│  │D-pad│   │Action│ │
│  │     │   │Btns  │ │
│  └─────┘   └──────┘ │
│   Level info / HUD   │
│                      │
└──────────────────────┘
```

- Game viewport: top of screen, full width, square.
- Controls area: below the game viewport, fills remaining height.
- D-pad on the left side, action buttons (Wait, Undo, Reset, Menu) on the
  right side.

### 5.3 Landscape Orientation

```
┌─────────────────┬──────────┐
│                 │          │
│  Square Game    │ Controls │
│  Viewport       │  Area    │
│  (puzzle        │ ┌──────┐ │
│   diorama)      │ │D-pad │ │
│                 │ └──────┘ │
│                 │ ┌──────┐ │
│                 │ │Action│ │
│                 │ │Btns  │ │
│                 │ └──────┘ │
│                 │  HUD     │
└─────────────────┴──────────┘
```

- Game viewport: left side of screen, square (height-matched).
- Controls area: right side, fills remaining width.

> **Open question:** In landscape, should the controls be on the right side
> only, or should D-pad be on the left and action buttons on the right (with
> the game viewport in the center)? The center-viewport option uses screen
> real estate better on wide devices but may make the game viewport smaller.

### 5.4 iPad Considerations

- iPads have a nearly-square aspect ratio in some models, leaving minimal
  controls area. Ensure minimum control zone sizes are enforced.
- On iPad with a connected keyboard or gamepad, the touch controls area can
  be hidden to give the full screen to the game viewport.

### 5.5 Desktop / Non-Touch Platforms

On desktop, Steam Deck, and Apple TV, the game viewport uses the **full
screen** (no controls area). All input comes from physical devices.

On Steam Deck, if the touchscreen is used, a minimal touch overlay may appear
temporarily but does not permanently reserve screen space.
