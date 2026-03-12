# 04 -- Overworld

## 1. Overview

The overworld is the level-selection system, presented as a series of
**per-biome diorama maps**. Each biome has its own self-contained diorama; the
player navigates between biomes via special transition nodes.

There is **no turn system** on the overworld -- this is **pure navigation**
with no puzzles or gating mechanics beyond star gates (see §7). The overworld
exists solely for level selection and atmosphere.

The visual and navigation style is inspired by **Super Mario World** and
**Baba Is You** -- paths between nodes can be long, winding, and aesthetically
interesting, not a rigid grid.

## 2. Biome Dioramas

Each biome's overworld is a **static diorama** (no scrolling or panning). The
entire biome map fits on screen at once, rendered in the same voxel art style
as the puzzle levels.

### 2.1 Visual Design

- Same matte low-poly voxel aesthetic as puzzle levels.
- Biome-appropriate theming (e.g. the Dark Forest overworld shows winding paths
  through twisted trees; the Mechanical Halls shows a workshop floor plan).
- Level nodes are visually distinct landmarks within the diorama.
- The player token (Theseus) is visible and moves along paths between nodes.
- Fun idle animations and decorative details are encouraged (e.g. birds, gears
  turning, water flowing), but these are purely cosmetic.

### 2.2 Node Visual States

Level nodes on the overworld diorama must communicate their state at a glance:

| State               | Visual Indicator                               |
|----------------------|-----------------------------------------------|
| Not yet reached      | Dimmed / locked appearance                     |
| Available (unbeaten) | Normal appearance, subtle pulsing or glow      |
| Beaten (1 star)      | Completed marker (e.g. flag, lit torch)        |
| Optimal (2 stars)    | Enhanced marker (e.g. golden flag, double torch) |

### 2.3 Diorama Scope

- Designed to hold ~10 level nodes + transition/gate nodes.
- No camera movement needed -- the full diorama is visible at the default
  camera framing.

## 3. Graph Structure

The overworld is internally a **graph**:

- **Nodes** represent a level, a biome transition, or a star gate.
- **Edges** connect adjacent nodes and define valid movement paths.
- Paths between nodes can be **arbitrarily long** visually (winding roads,
  bridges, tunnels) -- the token animates along the path.

### 3.1 Node Types

| Node Type         | Description                                            |
|-------------------|--------------------------------------------------------|
| Level Node        | Represents a puzzle level; can be incomplete or complete|
| Transition Node   | Moves the player to another biome's diorama            |
| Star Gate         | Blocks passage until the player has enough stars (see §7) |
| Secret Node       | Hidden level node; revealed by secret conditions (see §8) |

### 3.2 Cardinal Direction Navigation

Each node has **at most one exit per cardinal direction** (N/S/E/W). The player
presses a direction and the token follows the path leading in that direction to
the next node. This keeps navigation simple and unambiguous regardless of how
winding the visual path is.

- If no path exists in the pressed direction, the input is ignored.
- Paths are visually laid out so their departure direction matches the cardinal
  input that triggers them (e.g. a path going "up" from a node exits to the
  north).

### 3.3 Navigation Rules

The player stands on a node and can move to an adjacent node along any edge,
subject to these constraints:

1. **From a completed level node:** Move to any adjacent node (completed or not).
2. **From an incomplete level node:** Can only move to an **already-completed**
   adjacent node (i.e. you can retreat but not advance past an unsolved puzzle).
3. **Transition nodes:** Always traversable if the player can reach them. They
   transport the player to the corresponding node in the target biome.
4. **Star gates:** Passable only if the player's biome star count meets the
   gate's requirement (see §7).

> This means the player must solve levels to unlock forward progress, but can
> always backtrack through solved territory.

### 3.4 Graph Topology

- The graph is **not strictly linear** -- it may branch, offering the player
  a choice of which level to tackle next within a biome.
- Branches may converge, creating multiple valid orderings.
- At least one path through each biome must exist that visits all required
  levels.
- Secret nodes branch off the main path and are optional for progression.

## 4. Player Token

- Theseus is represented as a small figure on the overworld diorama.
- Movement between nodes is animated -- the token walks along the visual path
  (which may be long and winding).
- Movement is **free** (no turn system, no Minotaur on the overworld).
- Input: cardinal directions (at most one path per direction from each node).

## 5. Level Entry, Replay, and Auto-Progression

### 5.1 Entering a Level

- When Theseus stands on a level node, the player presses Confirm / Enter to
  begin the level.
- This works for both **new** (unbeaten) and **completed** levels. Completed
  levels can be replayed to improve turn count and earn the 2-star rating.
- Entering a level transitions from the overworld diorama to the puzzle
  diorama (see [05 -- UI/UX](05-ui-ux.md) for transition design).

### 5.2 Auto-Progression After Completion

After completing a level, the player **does not return to the overworld**.
Instead:

- The game automatically advances to the **next unbeaten level** in the biome.
- Levels have a numeric ordering within the biome (e.g. `dark-forest-01`,
  `dark-forest-02`, ... `dark-forest-10`).
- After beating the current level, the game finds the next numerically higher
  unbeaten level and loads it directly.
- The win transition plays, then the next puzzle's diorama loads.

> **Note:** Auto-progression only advances to unbeaten levels. If the player is
> replaying a completed level for a better score, the win screen returns them
> to the overworld (since the "next" level may already be beaten).

### 5.3 Return to Overworld

The player returns to the overworld only when:

- They **choose** to from the pause menu during a puzzle.
- They have **beaten all levels** in the biome (or all reachable levels on the
  current path).
- There is no next unbeaten level to auto-progress to.
- They just **replayed** an already-completed level.

When returning to the overworld, Theseus is placed on the node of the last
level played.

## 6. New Game Start: Tutorial Biome

A new game does **not** start on the overworld. Instead:

- The game begins directly in the **first puzzle** of the tutorial biome:
  **Ship of Theseus** (a ship-themed diorama).
- The tutorial biome contains a small number of introductory puzzles that teach
  core mechanics (movement, waiting, using walls against the Minotaur).
- No environmental features are introduced in the tutorial.
- After completing the tutorial puzzles, the player arrives at the tutorial
  biome's overworld diorama (the ship deck) and walks to a transition node
  (the gangplank) to disembark onto the island of Crete.
- This transition takes the player to the first "real" biome (Stone Labyrinth).

See [03 -- Level Design](03-level-design.md) §1.1 for the tutorial biome
description.

## 7. Star System and Biome Gating

### 7.1 Stars Per Level

Each level awards stars based on performance:

| Stars | Requirement                                          |
|-------|------------------------------------------------------|
| 0     | Level not yet completed                               |
| 1     | Level completed (any turn count)                      |
| 2     | Level completed in the **optimal** (minimum) turn count |

- Stars are cumulative -- once earned, they are kept even if the player later
  replays with a worse score.
- The optimal turn count is a property of the level data (provided by the
  external level generator).

### 7.2 Star Tracking

Stars are tracked at two levels:

- **Per-biome star count:** Total stars earned across all levels in that biome.
  Displayed as "X / N" (e.g. "14 / 20" for a 10-level biome). Used for
  in-biome star gates.
- **Global star count:** Total stars across all biomes. Used for certain secret
  reveal conditions (see §8).

### 7.3 Star Gates

**Star gates** are special nodes on the overworld that block passage until the
player has accumulated enough **biome-local stars**.

- Each star gate has a **star threshold** (e.g. "6 stars required").
- Only stars from the **current biome** count toward that biome's gates.
- Star gates can guard biome transition nodes, effectively gating access to the
  next biome.
- Star gates can also guard secret levels or optional paths within a biome.

### 7.4 Progression Implications

- Simply completing all levels in a biome gives 1 star each (~10 stars).
- This should be more than enough to pass most gates, making gates passable
  for casual players.
- Achieving 2-star optimal solutions is a challenge for completionists, not a
  requirement for basic progression.
- Gate thresholds should be tuned so that a player who solves most levels can
  always progress, even without optimal solutions.

> **TBD:** Exact star thresholds per gate will be tuned during level design.

## 8. Secret Levels

### 8.1 Secret Reveal Conditions

Secret nodes are initially **hidden** on the overworld diorama (invisible and
inaccessible). They become revealed when the player meets a configured
condition. Each secret can use any of the following triggers:

| Trigger Type           | Description                                          |
|------------------------|------------------------------------------------------|
| Biome star threshold   | Reach N stars within the current biome               |
| Global star threshold  | Reach N stars across all biomes combined              |
| Specific level optimal | Achieve the optimal (2-star) solution on a particular level |

### 8.2 Reveal Behavior

- When a secret's condition is met, the secret node **appears** on the
  overworld diorama with an animation (e.g. a hidden path materializes, a
  door opens in the scenery).
- A secret path connects the newly revealed node to an existing node in the
  graph.
- Once revealed, a secret node behaves like any other level node (can be
  entered, replayed, earns stars).
- Secret node stars **do count** toward the biome total.

### 8.3 Configuration

Secret node reveal conditions are defined in the biome's `overworld.yml`. See
[09 -- Content Pipeline](09-content-pipeline.md) §7 for the data format.

Multiple trigger types can be defined per secret if needed (any one being met
reveals the node).

## 9. Overworld Coordinate System

Overworld node positions and path waypoints use a **logical grid coordinate
system** (col, row) rather than pixel coordinates. Each biome's overworld has
a defined grid size (e.g. 8x6). The engine maps grid coordinates to
world-space diorama positions at runtime.

This keeps authoring data clean, resolution-independent, and logically aligned
without requiring a visual editor. See [09 -- Content Pipeline](09-content-pipeline.md)
§7.1 for details.
