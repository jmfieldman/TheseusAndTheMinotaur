# 01 -- Core Mechanics

## 1. Grid

- Rectangular grid of **N x M** tiles, where N and M each range from **4 to 16**.
- Dimensions are independent (e.g. a 4x12 puzzle is valid).
- **Coordinate convention:** Columns increase left-to-right (West → East).
  Rows increase bottom-to-top (South → North). **South** is the camera-facing
  edge (bottom of screen). **North** is the back wall (top/far edge). This
  convention is used consistently across all design docs and code.
- Walls exist on **edges between adjacent tiles**, not on tiles themselves.
  - A wall blocks movement between the two tiles it separates.
  - Outer boundary of the grid is implicitly walled, with two exceptions:
    the **entrance door** and the **exit door** (see §7).
- Tiles may be marked **impassable** (environment tiles). Neither Theseus nor
  the Minotaur can enter an impassable tile. They are functionally equivalent
  to a tile walled on all sides but are rendered as biome-appropriate blocking
  geometry (e.g. deep water, lava, large columns). See
  [09 -- Content Pipeline](09-content-pipeline.md) §2.4.

## 2. Actors

There are exactly **two actors** on the board at all times:

| Actor    | Controlled By | Count | Mortal |
|----------|---------------|-------|--------|
| Theseus  | Player        | 1     | Yes -- can be killed by Minotaur or environmental hazards |
| Minotaur | AI (deterministic) | 1 | **No** -- the Minotaur is always alive |

No level will ever contain more than one Minotaur. The Minotaur **cannot be
killed or incapacitated** by any environmental feature. Environmental features
may **block** the Minotaur's movement (acting as walls), but the Minotaur is
never removed from the board and never has its steps forfeited by a feature.
There is no alternate win condition based on eliminating the Minotaur.

## 3. Turn Cycle

Each turn follows a strict three-phase cycle:

```
1. Theseus Phase      -- Player chooses an action
2. Environment Phase  -- All environmental features resolve (see §6)
3. Minotaur Phase     -- Minotaur takes up to 2 steps
```

The cycle repeats until a win or loss condition is met.

### 3.1 Loss Checks

Loss conditions are checked at **multiple points** within the turn cycle:

- **After Theseus Phase:** If Theseus moved onto the Minotaur's tile, Theseus
  loses immediately (environment and Minotaur phases do not occur).
- **After Environment Phase:** If an environmental hazard kills Theseus (e.g.
  spikes activate on his tile, weak floor collapses), Theseus loses before the
  Minotaur phase begins.
- **After Minotaur Step 1:** If the Minotaur occupies Theseus' tile, Theseus
  loses immediately (step 2 does not occur).
- **After Minotaur Step 2:** If the Minotaur occupies Theseus' tile, Theseus
  loses.

## 4. Theseus Actions

On his turn, Theseus may perform exactly one of:

| Action    | Effect                                    |
|-----------|-------------------------------------------|
| Move N/S/E/W | Move one tile in the chosen cardinal direction (blocked by walls) |
| Wait      | Stay in place; turn still advances         |
| Undo      | Revert the *entire* last turn (Theseus + Environment + Minotaur) |
| Reset     | Restart the level from its initial state (see §7.5 for reset animation) |
| Menu      | Open the pause/options menu (does not advance the turn) |

- **Move** and **Wait** advance the turn (triggering Environment + Minotaur phases).
- **Undo** rolls back one full turn cycle. Multiple undos can chain.
- **Reset** returns the level to turn 0.
- Attempting to move into a wall is a no-op (does not consume the turn).

## 5. Minotaur Movement

The Minotaur takes **up to 2 steps** per turn. Each step follows these rules
in order:

### 5.1 Step Algorithm (per step)

```
1. Compute horizontal_delta = Theseus.col - Minotaur.col
2. Compute vertical_delta   = Theseus.row - Minotaur.row

3. If horizontal_delta != 0:
     target = Minotaur position + sign(horizontal_delta) horizontally
     If no wall blocks this move:
       Minotaur moves to target
       DONE (step consumed)

4. If vertical_delta != 0:
     target = Minotaur position + sign(vertical_delta) vertically
     If no wall blocks this move:
       Minotaur moves to target
       DONE (step consumed)

5. Minotaur cannot move -- step is forfeited.
```

### 5.2 Key Properties

- **Horizontal priority:** The Minotaur always attempts horizontal movement
  before vertical.
- **Greedy / no pathfinding:** The Minotaur can only move in a direction that
  reduces its distance to Theseus. It will never move away or sideways to
  navigate around a wall.
- **Forfeited steps:** If walls block all closing moves, that step is lost
  (the Minotaur does not accumulate missed steps).
- **Intermediate capture:** If the Minotaur lands on Theseus' tile after
  step 1, the player loses immediately -- the Minotaur does not take step 2.
- **Environmental immunity:** The Minotaur ignores all hazards that would kill
  Theseus. Features may **block** the Minotaur's movement (e.g. a wall that
  appears during environment phase), but the Minotaur is never frozen, stunned,
  or otherwise incapacitated.

## 6. Environment Phase

Between the Theseus phase and the Minotaur phase, all environmental features
resolve simultaneously. Environmental features are **deterministic** and may
include multi-turn state machines (e.g. spikes on a 3-turn cycle).

> Full catalog of environmental features will be defined in
> [03 -- Level Design](03-level-design.md) as they are designed.

### 6.1 Design Constraints for Environmental Features

All environmental features must be:

- **Deterministic** -- given the same board state and turn number, the outcome
  is always identical.
- **Simultaneous** -- all features resolve at the same time within the
  environment phase (no ordering dependencies between features).
- **Visible** -- the player must be able to predict what will happen (state is
  never hidden).
- **Asymmetric (actor effects):** Features may affect Theseus and the Minotaur
  differently. Theseus can be killed by hazards; the Minotaur cannot. Features
  may block the Minotaur's movement (like walls) but never kill, freeze, or
  incapacitate it.

### 6.2 Environmental Kill Examples

- **Spike trap:** Theseus steps on a spike trap tile, then leaves or waits.
  The spikes shoot up during the environment phase. If Theseus is on the tile
  (because he waited), he dies. If he left, the spikes remain up for one turn
  — moving back onto the tile during that turn is also fatal.
- **Crumbling floor:** After Theseus steps off a crumbling floor tile, it
  collapses into a pit. If Theseus waits on it instead of leaving, it
  collapses and he falls.
- **Medusa wall:** If Theseus moves toward a Medusa face while in its line of
  sight (i.e. faces the Medusa head-on), he is killed immediately.
- **Moving platform gap:** If Theseus steps onto a pit tile with no platform
  present, he falls.
- **Ice slide hazard:** If Theseus slides across ice into an active hazard
  (e.g. active spike trap), he dies at the hazardous tile.

In all cases, the Minotaur is **unaffected** by these same hazards.

## 7. Entrance and Exit Doors

Each level has an **entrance door** and an **exit door**, both located on the
outer boundary of the grid (openings in the boundary wall).

### 7.1 Door Placement Rules

- Doors are placed on the **left side**, **right side**, or **top (back) wall**
  of the grid. Doors are **never** on the bottom (camera-facing) wall.
- The entrance and exit are always on **different** wall segments (never the
  same opening).

### 7.2 Entrance Door

- Theseus **enters the level** by walking in through the entrance door from
  outside the grid.
- Once Theseus steps onto the first interior tile, the entrance door
  **closes and locks behind him** with a biome-themed lock animation (e.g.
  stone door slides shut, iron bars drop down, vines seal the opening).
- After locking, the entrance functions as a normal wall segment for the
  remainder of the level.

### 7.3 Exit Door

- The exit door is an **opening in the boundary wall** leading to a
  **virtual exit tile** that sits just outside the grid boundary.
- The virtual exit tile is visible to the player (the floor extends one tile
  outward through the door opening), but is **outside the playable grid**.
- **The Minotaur cannot pass through the exit door.** The exit boundary edge
  acts as a **wall for the Minotaur** -- only Theseus can step through it.
  The Minotaur is confined to the N×M grid at all times.
- Environmental features likewise cannot affect the virtual exit tile.
- The exit door can have the god-light effect (see
  [02 -- Visual Style](02-visual-style.md) §4.4) shining through the opening.
- If the exit is on the **top (back) wall**, the wall geometry around the
  exit opening should be **transparent or cut away** so the player can see
  Theseus stepping through it from the fixed camera angle.

### 7.4 Win Condition

Theseus wins **instantly** when he steps onto the virtual exit tile:

1. Theseus moves through the exit door opening onto the virtual exit tile.
2. The level is immediately won -- **no Environment Phase or Minotaur Phase
   occurs** after this move.
3. The Minotaur's position is irrelevant; the win is instant.

This replaces the previous design where Theseus had to survive a full turn
cycle on an exit tile. The new model is simpler and creates a satisfying
"escape" moment.

### 7.5 Level Start and Reset Sequence

When a level first loads (or is reset), the following sequence plays:

1. The diorama is shown with the entrance door open and the exit door visible.
   The Minotaur and Theseus are **not** on the board initially.
2. The **environment resets** -- all environmental features return to their
   initial states (with animation if mid-level reset).
3. The **Minotaur drops in** from above, landing on his starting tile with a
   ground-shake impact.
4. **Theseus enters** through the entrance door, hopping onto the first
   interior tile.
5. The entrance door **closes and locks** behind Theseus.
6. Player control begins.

On a **mid-level reset**, before the above sequence plays:

- Theseus and the Minotaur **lift up into the sky** and disappear (quick
  upward tween to off-screen).
- Then steps 2--6 above play out.

## 8. Loss Conditions

Theseus loses when any of the following occur (note: none of these can occur
on the virtual exit tile, since stepping onto it is an instant win):

1. **Theseus walks into Minotaur:** If Theseus moves onto the Minotaur's tile
   during the Theseus phase, Theseus dies immediately (before Environment or
   Minotaur phases occur).
2. **Minotaur capture:** The Minotaur occupies the same tile as Theseus at any
   point during the Minotaur phase (after step 1 or step 2).
3. **Environmental death:** An environmental hazard kills Theseus during the
   Environment phase (see §6.2).

## 9. Undo System

- Full turn-granularity undo (rolls back Theseus move + environment state +
  Minotaur moves).
- **Unlimited** undo depth -- can always undo back to turn 0.
- No undo limit as a difficulty lever; difficulty comes purely from the puzzle
  design itself.
- Implementation: maintain a stack of complete board snapshots, or a stack of
  deltas.

## 10. Input Buffering and Animation

The game logic and animation systems are **decoupled** to keep input feeling
responsive without sacrificing visual clarity.

### 10.1 Core Model

- Game logic resolves **instantly** when the player commits an action (Theseus
  move + environment resolution + Minotaur 2 steps all computed immediately).
- The renderer then **plays back** the visual sequence in order:
  1. Theseus move animation
  2. Environment phase animations
  3. Minotaur step 1 animation
  4. Minotaur step 2 animation (if applicable)
- Animations are **never fast-forwarded or skipped**. Every animation plays
  out fully so the player can always see what happened.

### 10.2 Input Buffer Window

During most of the animation sequence, **new input is ignored**. The player
cannot queue a move during Theseus's animation, the environment phase, or the
Minotaur's first step.

The **buffer window** opens during the **Minotaur's last step animation**:

- If the Minotaur takes 2 steps: window opens at the start of step 2's
  animation.
- If the Minotaur takes 1 step: window opens at the start of step 1's
  animation (it is the last step).
- If the Minotaur takes 0 steps: no window. Input is accepted only after all
  animations complete (Theseus move + environment). This is fine because the
  total animation is short.

During the buffer window, a **fresh key press** (not a held key) is stored as
the buffered action. If the player presses multiple keys during the window,
**last press wins** -- the buffer is overwritten until it fires.

**Held keys** (keys that were already down before the buffer window opened)
are ignored. Only a new key-down event (key was up, then pressed) registers.
This prevents accidental rapid-fire moves from holding a direction.

### 10.3 Buffer-Eligible Actions

The following actions can be buffered during the Minotaur's last step:

| Action              | Bufferable | Notes |
|---------------------|------------|-------|
| Move (N/S/E/W)      | Yes        | Resolves next turn normally |
| Wait                | Yes        | Resolves next turn normally |
| Undo                | Yes        | Reverts the current turn |
| Reset               | Yes        | Restarts the level |
| Pause               | No         | Takes effect immediately at any time |

**Pause** is special -- it is always accepted immediately regardless of
animation state (it opens a menu overlay, not a game action).

### 10.4 Buffer Commit

When the Minotaur's last step animation completes:

1. If a buffered action exists: the next turn's logic resolves instantly using
   the buffered action, and the new turn's animations begin playing.
2. If no buffered action exists: the game waits for player input normally.

The buffered action is **not pre-validated**. If the player buffers "move east"
and that would walk Theseus into the Minotaur, the turn resolves normally and
Theseus dies. The buffer just accepts intent; the game logic evaluates it.

### 10.5 Death State

If the resolved game state is a **loss** (Minotaur capture or environmental
death):

- **Movement input is blocked.** The player cannot queue new move/wait actions.
- The **death animation** plays (e.g. Theseus voxels explode outward).
- **Undo and Reset remain available** even in death state:
  - **Undo** reverses the death animation (e.g. voxels reconstitute back into
    Theseus's body) and rewinds the turn, restoring the pre-death board state.
  - **Reset** restarts the level from the initial state (with the standard
    reset animation sequence from §7.5).
- After the death animation completes, the **loss UI** appears with options
  to Undo, Reset, or Quit.

### 10.6 Benefits

- The game never feels sluggish -- experienced players can chain moves with
  minimal downtime between turns.
- All animations play fully, so players always understand what happened.
- The "fresh press only" rule prevents accidental moves from held keys while
  still allowing deliberate rapid play.
- Undo from death creates a satisfying "rewind" feel.
