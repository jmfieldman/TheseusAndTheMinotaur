# 04 -- Overworld

## 1. Overview

The overworld is the level-selection system, presented as a series of
**per-biome diorama maps**. Each biome has its own self-contained diorama; the
player navigates between biomes via special transition nodes.

There is **no turn system** on the overworld -- this is **pure navigation**
with no puzzles or gating mechanics. The overworld exists solely for level
selection and atmosphere.

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

### 2.2 Diorama Scope

- Designed to hold ~10 level nodes + transition nodes to other biomes.
- No camera movement needed -- the full diorama is visible at the default
  camera framing.

## 3. Graph Structure

The overworld is internally a **graph**:

- **Nodes** represent either a **level** or a **biome transition point**.
- **Edges** connect adjacent nodes and define valid movement paths.

### 3.1 Node Types

| Node Type         | Description                                            |
|-------------------|--------------------------------------------------------|
| Level Node        | Represents a puzzle level; can be incomplete or complete|
| Transition Node   | Moves the player to another biome's diorama            |
| Secret Node       | Hidden level node; becomes visible upon some condition  |

### 3.2 Navigation Rules

The player stands on a node and can move to an adjacent node along any edge,
subject to these constraints:

1. **From a completed level node:** Move to any adjacent node (completed or not).
2. **From an incomplete level node:** Can only move to an **already-completed**
   adjacent node (i.e. you can retreat but not advance past an unsolved puzzle).
3. **Transition nodes:** Always traversable if the player can reach them. They
   transport the player to the corresponding node in the target biome.

> This means the player must solve levels to unlock forward progress, but can
> always backtrack through solved territory.

### 3.3 Graph Topology

- The graph is **not strictly linear** -- it may branch, offering the player
  a choice of which level to tackle next within a biome.
- Branches may converge, creating multiple valid orderings.
- At least one path through each biome must exist that visits all required
  levels.
- Secret nodes branch off the main path and are optional for progression.

## 4. Biome Unlock Rule

Biome unlocking is **graph-gated**: if the player can physically reach a
transition node (by solving the levels along the path to it), they can travel
to the next biome. There is no separate "beat X levels" threshold or linear
unlock.

This means:

- Biome transitions can be placed anywhere in the graph, not necessarily at the
  "end" of a biome.
- A biome could have multiple transition nodes leading to different biomes.
- The level designer controls the unlock pace entirely through graph layout.

## 5. Player Token

- Theseus is represented as a small figure on the overworld diorama.
- Movement between nodes is animated (walking/sliding along the edge path).
- Movement is **free** (no turn system, no Minotaur on the overworld).
- Input: cardinal directions or direct node selection (see [06 -- Input](06-input.md)).

## 6. Level Entry and Auto-Progression

### 6.1 Entering a Level

- When Theseus stands on a level node, the player can choose to enter the
  level.
- Entering a level transitions from the overworld diorama to the puzzle
  diorama (see [05 -- UI/UX](05-ui-ux.md) for transition design).

### 6.2 Auto-Progression After Completion

After completing a level, the player **does not return to the overworld**.
Instead:

- The game automatically advances to the **next unbeaten level** in the biome.
- Levels have a numeric ordering within the biome (e.g. `dark-forest-01`,
  `dark-forest-02`, ... `dark-forest-10`).
- After beating the current level, the game finds the next numerically higher
  unbeaten level and loads it directly.
- The win transition plays, then the next puzzle's diorama loads.

### 6.3 Return to Overworld

The player returns to the overworld only when:

- They **choose** to from the pause menu during a puzzle.
- They have **beaten all levels** in the biome (or all reachable levels on the
  current path).
- There is no next unbeaten level to auto-progress to.

When returning to the overworld, Theseus is placed on the node of the last
level played.

## 7. Progression State

Per save slot, the overworld tracks:

- Which levels have been completed.
- Which biomes have been visited / unlocked.
- Which secret nodes have been revealed.
- The player's current overworld position (biome + node).

> See [05 -- UI/UX](05-ui-ux.md) §3 for save system details.
