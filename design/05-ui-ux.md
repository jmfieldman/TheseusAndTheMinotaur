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

### 1.1 New Game Flow

A new game skips the overworld entirely and starts in the first tutorial
puzzle (Ship of Theseus biome). See [04 -- Overworld](04-overworld.md) §6.

```
New Game ──► Tutorial Puzzle 1 ──► ... ──► Tutorial Complete
                                                │
                                           Ship Overworld
                                           (walk to gangplank)
                                                │
                                           Stone Labyrinth
                                           Overworld
```

### 1.2 Puzzle Auto-Progression

After completing a puzzle, the player does **not** return to the overworld.
Instead, the game automatically loads the next unbeaten level in the biome
(see [04 -- Overworld](04-overworld.md) §5.2). The flow during a play session
typically looks like:

```
Overworld ──► Puzzle 01 ──(win)──► Puzzle 02 ──(win)──► Puzzle 03 ──► ...
                                                           │
                                                     (all beaten or
                                                      player exits or
                                                      replaying level)
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
- **Delete slot** available with a confirmation dialog ("Are you sure? This
  cannot be undone.").
- Settings are stored in a **separate global file** (not per save slot), so
  they persist across all slots and are not lost on deletion. See
  [09 -- Content Pipeline](09-content-pipeline.md) §8.1.

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

### 5.3 Touch / iOS Layout (Portrait Only)

On iOS/iPadOS (portrait orientation), the screen is divided into two zones:

- **Game area:** A **square region** at the top of the screen containing the
  puzzle diorama.
- **Controls area:** The remaining space below, containing:
  - D-pad (left side)
  - Wait, Undo, Reset, Menu buttons (right side)
  - Turn counter, level info, and star display

This layout ensures the game view is never obscured by controls. See
[06 -- Input](06-input.md) §5 for detailed layout specifications.

The HUD does **not** show Minotaur AI information (e.g. predicted movement
direction). The Minotaur's rules are simple and deterministic enough that
players can predict its behavior without assistance.

## 6. Transitions

All transitions between overworld and puzzle use a **seamless zoom** system.
Because the overworld contains miniature LOD versions of every puzzle diorama,
transitions are smooth camera movements rather than cuts or dissolves.

### 6.1 Level Enter (from Overworld)

1. Player confirms entry on a level node (mini-diorama).
2. Camera **zooms in** toward the mini-diorama. The orthographic projection
   bounds smoothly interpolate from overworld framing to puzzle framing.
3. During the zoom, the **low-detail LOD mesh fades** to the **high-detail
   puzzle mesh** (quick crossfade once the diorama fills enough of the
   screen that detail matters).
4. On arrival at full zoom, the **level start sequence** plays:
   - Minotaur drops from above onto starting tile (ground shake).
   - Theseus hops in through the entrance door.
   - Entrance door locks shut.
5. Player control begins.

### 6.2 Level Win → Next Level (Auto-Progression)

1. **Win animation:** Theseus steps through the exit door onto the virtual
   exit tile. Brief celebration pose.
2. **Results display** (overlay):
   - Turn count for this attempt
   - Best (lowest) turn count ever achieved
   - Stars earned: ★ for completion, ★★ for optimal move count
   - New star indicator if this is the first time earning a star tier
3. Camera **zooms out** from the completed puzzle to a mid-level overworld
   view. The high-detail mesh fades back to LOD during zoom-out.
4. The completed node's visual state updates (flag / torch appears on the
   mini-diorama).
5. Camera **pans** across the overworld to the next unbeaten puzzle node.
6. Camera **zooms in** to the next puzzle. LOD fades to high-detail.
7. Level start sequence plays (Minotaur drop, Theseus entrance, door lock).

The player can press Confirm at any point to accelerate the transition.

### 6.3 Level Win → Overworld (Biome Complete / Replay)

1. Same win animation + results display as §6.2.
2. Camera **zooms out** all the way to the full overworld view.
3. Completed node's visual state updates.
4. Theseus token appears on the completed node.

### 6.4 Level Exit (Environmental Death)

- Environmental hazard kills Theseus (spikes, pit, etc.) -- brief death
  animation.
- Prompt: Retry / Undo Last Move / Return to Overworld.
- **Retry** triggers the reset sequence (actors lift off, environment resets,
  Minotaur drops in, Theseus re-enters through entrance door).
- **Return to Overworld** triggers a zoom-out transition.

### 6.5 Level Exit (Minotaur Capture)

- Minotaur catches Theseus -- brief dramatic animation.
- Prompt: Retry / Undo Last Move / Return to Overworld.
- Same retry/return behavior as §6.4.

### 6.6 Level Exit (Abandon)

- From pause menu: return to overworld via zoom-out. No save of in-progress
  state (level resets next time).

### 6.7 Biome Transition

- When using a transition node on the overworld, the current biome diorama
  transitions out and the new biome diorama transitions in.
- Could be a dissolve, a camera pan through a doorway/portal, or a diorama
  swap animation. All mini-dioramas for the new biome are generated during
  the transition loading screen.

## 7. Pause Menu

Accessible at any time during a puzzle (does not advance the turn):

- **Resume**
- **Settings** (subset: audio)
- **Return to Overworld** (confirms abandonment)
- **Return to Title**

## 8. Win / Completion Screen

> **TBD:** What happens when the player completes all levels? Credits sequence?
> Final diorama scene? Unlockable content?
