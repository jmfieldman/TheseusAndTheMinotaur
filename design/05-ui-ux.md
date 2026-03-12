# 05 -- UI / UX

## 1. Screen Flow

```
┌─────────────┐
│ Title Screen │
│  Play        │──► Save Slot Select ──► Overworld ──► Puzzle Level
│  Continue    │──► Overworld (last save position)     │
│  Settings    │──► Settings Screen                    ▼
│  Quit        │                                   Pause Menu
└─────────────┘                                    │  Resume
                                                   │  Settings
                                                   │  Return to Overworld
                                                   │  Return to Title
```

### 1.1 Puzzle Auto-Progression

After completing a puzzle, the player does **not** return to the overworld.
Instead, the game automatically loads the next unbeaten level in the biome
(see [04 -- Overworld](04-overworld.md) §6.2). The flow during a play session
typically looks like:

```
Overworld ──► Puzzle 01 ──(win)──► Puzzle 02 ──(win)──► Puzzle 03 ──► ...
                                                           │
                                                     (all beaten or
                                                      player exits)
                                                           │
                                                           ▼
                                                       Overworld
```

## 2. Title Screen

- **Play** -- go to save slot selection.
- **Continue** -- shown only if a save is in progress; jumps directly to the
  overworld at the last saved position.
- **Settings** -- opens settings screen.
- **Quit** -- exit the game (not shown on platforms where quit is handled by OS,
  e.g. iOS/tvOS).

### 2.1 Title Screen Presentation

- Full-screen diorama backdrop (a hero voxel scene -- possibly animated with
  subtle idle motion).
- Game logo rendered as 3D voxel text or a stylized emblem.
- Menu items overlaid or integrated into the diorama scene.

## 3. Save System

### 3.1 Save Slots

- **3 save slots** available.
- Each slot displays:
  - Biome name / level last played
  - Progression percentage (levels completed / total)
  - Play time
  - Empty slots show "New Game"

### 3.2 Save Behavior

- **Auto-save** on level completion and biome transitions.
- No mid-level save (levels are short enough that this is unnecessary).
- Selecting an occupied save slot resumes from last auto-save.
- Selecting an empty slot starts a new game.

> **Open question:** Allow deleting save slots? If so, require confirmation.

## 4. Settings Screen

| Category  | Settings                                         |
|-----------|--------------------------------------------------|
| Audio     | Music volume, SFX volume                         |
| Display   | Fullscreen/windowed (desktop), resolution (desktop) |
| Input     | Control scheme selection, button remapping        |

> **TBD:** Accessibility options (colorblind palette, high-contrast mode,
> screen reader hints).

## 5. Puzzle HUD

During a puzzle level, the HUD should be **minimal** to keep focus on the
diorama:

### 5.1 Always Visible

- **Turn counter** (small, corner of screen)
- **Level name / identifier** (small, top of screen)

### 5.2 Available via Input (Not Always Visible)

- **Undo button** (on touch platforms, always visible)
- **Reset button**
- **Menu button**

### 5.3 Touch / iOS Layout

On iOS/iPadOS, the screen is divided into two zones:

- **Game area:** A **square region** containing the puzzle diorama. Positioned
  at the top in portrait, or on one side in landscape.
- **Controls area:** The remaining rectangular strip, containing:
  - D-pad / directional controls
  - Wait, Undo, Reset buttons
  - Menu button
  - Turn counter and level info

This layout ensures the game view is never obscured by controls. See
[06 -- Input](06-input.md) §5 for detailed layout specifications.

> **Open question:** Should the HUD show any Minotaur AI information (e.g.
> an indicator of which direction the Minotaur will move next)?

## 6. Transitions

### 6.1 Level Enter (from Overworld)

Transition from overworld to puzzle. Options:

- Camera zooms into the level node, diorama assembles/rises from the node.
- Quick crossfade/dissolve.

### 6.2 Level Win → Next Level (Auto-Progression)

- Victory animation on the diorama (e.g. Theseus celebrates, diorama sparkles).
- Brief results display (turns taken, optional stats).
- Crossfade/transition directly to the next puzzle diorama (no overworld visit).

### 6.3 Level Win → Overworld (Biome Complete)

- Same victory animation + results display.
- Transition back to overworld with the completed node visually updated.

### 6.4 Level Exit (Lose)

- Minotaur catches Theseus or environmental hazard kills Theseus -- brief
  dramatic animation.
- Prompt: Retry / Undo Last Move / Return to Overworld.

### 6.5 Level Exit (Abandon)

- From pause menu: return to overworld. No save of in-progress state (level
  resets next time).

### 6.6 Biome Transition

- When using a transition node on the overworld, the current biome diorama
  transitions out and the new biome diorama transitions in.
- Could be a dissolve, a camera pan through a doorway/portal, or a diorama
  swap animation.

## 7. Pause Menu

Accessible at any time during a puzzle (does not advance the turn):

- **Resume**
- **Settings** (subset: audio)
- **Return to Overworld** (confirms abandonment)
- **Return to Title**

## 8. Win / Completion Screen

> **TBD:** What happens when the player completes all levels? Credits sequence?
> Final diorama scene? Unlockable content?
