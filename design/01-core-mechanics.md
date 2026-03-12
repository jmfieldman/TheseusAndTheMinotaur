# 01 -- Core Mechanics

## 1. Grid

- Rectangular grid of **N x M** tiles, where N and M each range from **4 to 16**.
- Dimensions are independent (e.g. a 4x12 puzzle is valid).
- Walls exist on **edges between adjacent tiles**, not on tiles themselves.
  - A wall blocks movement between the two tiles it separates.
  - Outer boundary of the grid is implicitly walled.
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
| Reset     | Restart the level from its initial state   |
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

- **Spike trap (active):** If Theseus is standing on a spike tile when it
  activates during environment phase, Theseus dies.
- **Weak floor:** After Theseus steps on it, it collapses during the next
  environment phase. If Theseus is still on it, he falls.
- **Crushing wall:** A wall segment that moves into a tile -- if Theseus
  occupies that tile, he is killed.

In all cases, the Minotaur is **unaffected** by these same hazards.

## 7. Win Condition

Theseus wins when:

1. Theseus occupies the **exit tile**, AND
2. The Minotaur has completed both of its steps for that turn, AND
3. The Minotaur is **not** on the exit tile.

In other words, Theseus must survive the full turn cycle while on the exit.

## 8. Loss Conditions

Theseus loses when any of the following occur:

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

## 10. Non-Blocking Input Model

The game logic and animation systems are **decoupled** to keep input feeling
responsive:

- Game logic resolves **instantly** when the player commits an action (Theseus
  move + environment resolution + Minotaur 2 steps all computed immediately).
- The renderer then **plays back** the visual sequence (Theseus animation,
  environment animation, Minotaur step 1 animation, Minotaur step 2 animation).
- **The player may input their next action at any time**, even while the
  previous turn's animations are still playing, provided the new action would
  be legal and the player has not entered a death state.
- When a new action is received during animation playback:
  - All pending animations for the previous turn are **fast-forwarded** to
    their final positions.
  - The new turn's logic resolves immediately.
  - The new turn's animations begin playing.
- **Death state blocks input:** If the resolved game state is a loss (Minotaur
  capture or environmental death), further gameplay input is blocked. The death
  animation plays, then the loss UI appears.

### 10.1 Benefits

- Experienced players can input moves as fast as they think -- no waiting for
  animations.
- New players who input slowly will see full animations play out naturally.
- The game never feels sluggish or unresponsive.
