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
| OW_MOVE_NORTH  | Move token north along path        |
| OW_MOVE_SOUTH  | Move token south along path        |
| OW_MOVE_EAST   | Move token east along path         |
| OW_MOVE_WEST   | Move token west along path         |
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

- **On-screen D-pad** for directional movement (no swipe gestures).
- **On-screen buttons** for Wait, Undo, Reset, Pause.
- All touch controls are placed in the **dedicated controls area** below the
  game viewport (portrait only; see §5).
- D-pad chosen over swipe for reliability and precision in tight puzzle
  situations.

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

### 5.1 Orientation: Portrait Only

iOS and iPadOS are locked to **portrait orientation**. This simplifies layout,
matches how most people hold their phones for puzzle games, and provides a
natural split between the game view and controls.

### 5.2 Layout

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
│  │D-pad│   │Wait  │ │
│  │     │   │Undo  │ │
│  └─────┘   │Reset │ │
│             │Menu  │ │
│             └──────┘ │
│   Level info / HUD   │
│                      │
└──────────────────────┘
```

- **Game viewport:** Top of screen, full width, **square**. Contains the
  puzzle diorama rendered with orthographic projection.
- **Controls area:** Below the game viewport, fills remaining height.
  - D-pad on the left side for directional movement.
  - Action buttons (Wait, Undo, Reset, Menu) on the right side.
  - Level info, turn counter, and star display in the controls area.

### 5.3 iPad Considerations

- iPads in portrait have a nearly-square aspect ratio, leaving a narrow
  controls strip. Ensure minimum control zone height is enforced.
- On iPad with a connected keyboard or gamepad, the touch controls area can
  be hidden and the game viewport expands to fill more of the screen.

### 5.4 Desktop / Non-Touch Platforms

On desktop, Steam Deck, and Apple TV, the game viewport uses the **full
screen** (no controls area). All input comes from physical devices.

On Steam Deck, if the touchscreen is used, a minimal touch overlay may appear
temporarily but does not permanently reserve screen space.
