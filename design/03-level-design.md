# 03 -- Level Design

## 1. Biomes

The game contains approximately **12 biomes**, all rooted in Greek mythology,
ancient Crete, and labyrinthine aesthetics.

### 1.1 Candidate Biome List

| #  | Biome                  | Description                                              | Role         |
|----|------------------------|----------------------------------------------------------|--------------|
| 0  | Ship of Theseus        | Wooden ship deck, rigging, sea backdrop; tutorial levels  | **Tutorial** |
| 1  | Stone Labyrinth        | Classic Cretan maze, sandstone and marble                | First biome  |
| 2  | Dark Forest            | Ancient twisted olive/cypress groves, overgrown ruins    |              |
| 3  | Mechanical Halls       | Bronze gears, Daedalus-style mechanisms and automata     |              |
| 4  | Infernal Dungeon       | Deep underground, lava cracks, Tartarus-inspired         |              |
| 5  | Sunken Ruins           | Flooded temple corridors, shallow water, mossy stone     |              |
| 6  | Palace of Knossos      | Ornate Minoan palace, painted columns, frescoes          |              |
| 7  | Catacombs              | Bone-lined corridors, dim torchlight, narrow passages    |              |
| 8  | Crystal Caverns        | Underground geode chambers, faint luminescent minerals   |              |
| 9  | Overgrown Temple       | Jungle reclaiming ancient architecture, vines and roots  |              |
| 10 | Volcanic Forge         | Hephaestus-inspired, molten metal channels, anvils       |              |
| 11 | Celestial Observatory  | Open-air stargazing platform, astrolabes, constellations |              |
| 12 | The Minotaur's Sanctum | Final biome, dark and foreboding, culminating encounter  | Final biome  |

The **Ship of Theseus** is a special tutorial biome with at most **3 levels**.
Tutorial levels are small (4x4 or 5x5) and teach core mechanics quickly:

1. Basic movement and reaching the exit door (Minotaur present but walled off
   in an unreachable area, so the player can focus on movement without threat)
2. How the Minotaur moves and using walls to avoid capture
3. The "wait" action and its tactical use

A new game starts directly in the first tutorial puzzle (not the overworld).
After completing the tutorial levels, the player reaches the ship's overworld
diorama (deck), walks to the gangplank transition node, and disembarks onto
the island of Crete (Stone Labyrinth biome). See
[04 -- Overworld](04-overworld.md) §6 for details.

> **TBD:** Final biome list pending creative review. Biomes may be added,
> removed, or reordered.

### 1.2 Biome Properties

Each biome defines:

- **Color sub-palette** (within the global muted/low-contrast constraints)
- **Voxel material set** (floor, wall, accent geometry)
- **Ambient sound profile** (see [07 -- Audio](07-audio.md))
- **Environmental features introduced** (see §3 below)
- **Diorama decoration set** (non-interactive voxel art around the puzzle grid)

## 2. Level Structure

### 2.1 Counts

- ~**10 levels per biome** (standard progression)
- **Secret levels** accessible from the overworld (see [04 -- Overworld](04-overworld.md))
- Total: **100--200 levels**

### 2.2 Difficulty Curve

Difficulty increases along two axes:

1. **Within a biome:** Levels 1--10 ramp from introductory to challenging for
   that biome's feature set.
2. **Across biomes:** Later biomes introduce more complex environmental features
   and larger grids.

Early biomes use smaller grids (4x4 to 8x8) and no/few environmental features.
Later biomes may use grids up to 16x16 with multiple interacting features.

### 2.3 Level Data

Levels are authored by an **external level generator** (not part of this repo).
The engine consumes a level data format that encodes:

- Grid dimensions (N x M)
- Wall placement (per-edge)
- Impassable tile positions (environment blocking tiles)
- **Entrance door** position and side (boundary wall segment where Theseus
  enters; closes and locks behind him)
- **Exit door** position and side (boundary wall segment where Theseus
  escapes; has a virtual exit tile outside the grid)
- Theseus start position (the interior tile adjacent to the entrance door)
- Minotaur start position
- Environmental feature placement and configuration
- Biome identifier (determines visual theme and procedural generation style)
- Optimal (minimum) turn count for star rating
- Metadata (level name, difficulty rating, author, etc.)

Door placement constraints: doors may be on the **left**, **right**, or
**top (back)** boundary walls only -- never on the **bottom (camera-facing)**
wall. See [01 -- Core Mechanics](01-core-mechanics.md) §7 for full rules.

> See [09 -- Content Pipeline](09-content-pipeline.md) for the level data format
> specification.

## 3. Environmental Features

Environmental features are tile-based or edge-based elements that add complexity
beyond basic walls. They resolve during the **Environment Phase** of each turn
(see [01 -- Core Mechanics](01-core-mechanics.md) §6).

### 3.1 Design Principles

All features must be:

- **Deterministic** -- same input state = same output state, always.
- **Predictable** -- the player can always deduce what will happen next by
  observing the current state.
- **Visually distinct** -- each feature must read clearly against the biome
  diorama.
- **Introduced gradually** -- each biome introduces at most 1--2 new features,
  with early levels in that biome serving as tutorials.

### 3.2 Feature Catalog

#### 3.2.1 Spike Trap

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Tile                                 |
| **Biome**   | Introduced in Dark Forest            |

**Behaviour:**

Spike traps are **reactive** — they respond to Theseus stepping on them, not
to a fixed cycle timer.

1. Spikes start **down** (safe). Theseus can step onto the tile freely.
2. After Theseus **leaves** the spike trap tile (or chooses Wait while on it),
   the spikes shoot **up** during the environment phase of that same turn.
3. Spikes remain **up** for exactly **one full turn** (one complete
   Theseus → Environment → Minotaur cycle), then retract back down during the
   environment phase of the following turn.
4. While spikes are **up**:
   - If Theseus moves onto or remains on the tile, **he dies**.
   - The tile is treated as **impassable for the Minotaur** — the Minotaur
     will not move onto a tile with active spikes (equivalent to a wall).
5. The Minotaur standing on a spike trap tile does **not** trigger it. Only
   Theseus triggers the activation.
6. If Theseus **waits** on a spike trap tile that is currently down, the spikes
   activate during the environment phase of that turn, killing Theseus
   immediately.

**Config (level JSON):**
- `"initial_active"`: bool — whether spikes start up (default: false)

#### 3.2.2 Pressure Plate

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Tile                                 |
| **Biome**   | Introduced in Mechanical Halls       |

**Behaviour:**

Pressure plates permanently toggle linked grid elements when stepped on by
Theseus. They act as switches — each activation flips the state of all linked
targets.

1. When Theseus steps onto the pressure plate tile, all linked targets
   **toggle** their state immediately (before environment phase).
2. The toggle is **permanent** — the state persists until another pressure
   plate changes it (or undo reverts it).
3. The Minotaur stepping on a pressure plate does **not** activate it.

**Linkable target types:**

| Target Type        | Toggle Effect                                      |
|--------------------|----------------------------------------------------|
| Wall segment       | Wall present ↔ wall absent (raise/lower a wall)    |
| Impassable tile    | Impassable ↔ passable (e.g. bridge appears/disappears) |

A single pressure plate can affect **multiple targets** simultaneously.

**Visual design:**

- Each pressure plate and its linked targets share a **subtle color tint**
  (e.g. blue plate → blue-tinted walls/tiles). This allows the player to
  visually associate plates with their effects when multiple plates exist in
  a single level.
- The plate has a visible pressed/unpressed state indicator.

**Config (level JSON):**
- `"color"`: string — tint color identifier (e.g. `"blue"`, `"red"`, `"green"`)
- `"targets"`: array of target objects, each with:
  - `"type"`: `"wall"` or `"tile"`
  - For walls: `"col"`, `"row"`, `"side"` — the wall edge to toggle
  - For tiles: `"col"`, `"row"` — the tile to toggle passability
  - `"initial_active"`: bool — whether the target starts in its active state

#### 3.2.3 Locking Gate

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Edge (wall segment)                  |
| **Biome**   | Introduced in Palace of Knossos      |

**Behaviour:**

A one-way passage that locks behind the actor who passes through it. Functions
as a door that bars close after any actor passes.

1. The gate starts **open** — either actor can pass through it from one side.
2. When any actor (Theseus or Minotaur) passes through the gate, it
   **locks permanently** — bars come up and the passage becomes a wall.
3. The lock is permanent for the remainder of the level (unless reversed by
   undo).

**Config (level JSON):**
- `"col"`, `"row"`, `"side"`: the wall edge where the gate exists
- `"passable_from"`: direction from which the gate can be initially traversed
  (e.g. `"east"` means the gate is on a cell's east edge and can be entered
  from the west side of that edge)

#### 3.2.4 Auto-Turnstile (Rotating Wall)

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Junction (corner of 4 tiles)         |
| **Biome**   | Introduced in Mechanical Halls       |

**Behaviour:**

An automatic turnstile sits at the **junction of four tiles** (the shared
corner point). It rotates 90° every turn during the environment phase, taking
everything with it.

1. The turnstile is positioned at a junction point (shared corner of up to 4
   tiles).
2. Each environment phase, the four tiles surrounding the junction **rotate
   90° clockwise** (or counter-clockwise, per config):
   - All **walls** between those four tiles rotate with them.
   - Any **actors** (Theseus or Minotaur) standing on one of the four tiles
     are **moved** to the rotated tile position.
   - Any **features** on those tiles rotate with them.
3. The rotation is automatic and occurs every turn regardless of actor
   positions.
4. If an actor is rotated onto a hazardous tile or into collision, standard
   loss rules apply.

**Config (level JSON):**
- `"junction_col"`, `"junction_row"`: the column and row of the junction
  point (the corner shared by tiles at (c,r), (c-1,r), (c,r-1), (c-1,r-1))
- `"direction"`: `"cw"` (clockwise) or `"ccw"` (counter-clockwise)

#### 3.2.5 Manual Turnstile

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Junction (corner of 4 tiles)         |
| **Biome**   | Introduced in Mechanical Halls       |

**Behaviour:**

Similar to the auto-turnstile, but player-activated. It rotates the **walls**
connected to a junction, but does **not** move the floor tiles or actors.

1. The turnstile sits at a junction point (shared corner of up to 4 tiles).
2. When Theseus attempts to move **into a wall** that is connected to the
   turnstile junction, instead of being blocked, the walls at that junction
   **rotate 90°** in the push direction. Theseus does not move — the push
   consumes his action.
3. Only **walls** rotate — floor tiles, features, and actors stay in place.
4. The Minotaur does **not** activate manual turnstiles. If the Minotaur
   attempts to move through a wall at a manual turnstile junction, it is
   simply blocked (the wall does not rotate).

**Config (level JSON):**
- `"junction_col"`, `"junction_row"`: the column and row of the junction point

#### 3.2.6 Teleporter Pair

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Tile (paired)                        |
| **Biome**   | Introduced in Celestial Observatory   |

**Behaviour:**

Two tiles linked as a teleporter pair. Stepping onto one instantly moves the
actor to the other.

1. When any actor (Theseus or Minotaur) moves onto a teleporter tile, they
   are **immediately transported** to the paired tile.
2. The teleportation is instant — no environment phase is consumed.
3. If the destination tile is occupied by the other actor, standard collision
   rules apply.
4. Teleportation does **not** chain — arriving at the paired tile does not
   trigger the pair's teleporter again.

**Config (level JSON):**
- `"pair_id"`: string — shared identifier linking the two teleporter tiles
- `"col"`, `"row"`: position of this teleporter tile

#### 3.2.7 Crumbling Floor

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Tile                                 |
| **Biome**   | Introduced in Sunken Ruins           |

**Behaviour:**

A weakened floor tile that can be traversed once, then collapses into a
bottomless pit.

1. The tile starts **intact** — any actor can step on it.
2. When an actor **leaves** the crumbling floor tile, it **collapses** during
   the environment phase, becoming an impassable/deadly pit.
3. If Theseus **waits** on a crumbling floor tile, it collapses during the
   environment phase — **Theseus dies** (falls into the pit).
4. If the Minotaur is on a crumbling floor tile when it collapses, the
   Minotaur is **unaffected** (immune to hazards) but the tile still becomes
   impassable — the Minotaur must have moved off before it collapsed.
5. Once collapsed, the tile is a permanent pit — impassable and deadly to
   Theseus.

**Config (level JSON):**
- No additional config needed (position only).

#### 3.2.8 Moving Platform

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Tile (over void)                     |
| **Biome**   | Introduced in Infernal Dungeon       |

**Behaviour:**

A floating tile that moves back and forth across a chasm of bottomless pit
tiles. Multiple moving platforms can orbit a ring of pit tiles.

1. The platform moves along a fixed **path** (a sequence of pit tile
   coordinates) during each environment phase, advancing one tile per turn.
2. At the end of the path, the platform **reverses direction** (ping-pong) or
   **loops** (circular), depending on config.
3. Any actor standing on the platform **moves with it**.
4. If the platform moves an actor into collision with the other actor,
   standard collision rules apply.
5. If Theseus steps onto a pit tile that has no platform, **he dies**.
6. The Minotaur treats pit tiles without platforms as impassable (will not
   step onto them).
7. The Minotaur treats pit tiles with platforms as passable (can step onto
   the platform).

**Config (level JSON):**
- `"path"`: array of `{col, row}` coordinates defining the movement path
- `"mode"`: `"pingpong"` or `"loop"`
- `"initial_index"`: int — starting position in the path (default: 0)

#### 3.2.9 Medusa Wall

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Wall decoration (directional hazard) |
| **Biome**   | Introduced in Overgrown Temple       |

**Behaviour:**

A Medusa face mounted on a wall that kills Theseus if he moves **toward** it
while in its line of sight.

1. The Medusa face is placed on a specific **wall segment** and faces a
   cardinal direction (into the grid).
2. It has **line of sight** along its facing direction — a straight line of
   tiles extending from the wall until blocked by another wall or the grid
   boundary.
3. If Theseus **moves toward** the Medusa (i.e., moves in the opposite
   direction to the Medusa's facing direction) while standing in its line of
   sight, **Theseus dies** at the start of his move.
4. Theseus can safely:
   - Move **away** from the Medusa (same direction as its facing)
   - Move **perpendicular** to the Medusa's line of sight
   - Stand still (Wait) in the line of sight
   - Be outside the line of sight entirely
5. The Minotaur is **immune** — the Medusa does not affect the Minotaur.
6. Line of sight is blocked by walls — a wall between Theseus and the Medusa
   breaks line of sight.

**Config (level JSON):**
- `"col"`, `"row"`, `"side"`: the wall the Medusa is mounted on
- `"facing"`: the cardinal direction the Medusa faces (into the grid)

#### 3.2.10 Ice Tile

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Tile                                 |
| **Biome**   | Introduced in Crystal Caverns        |

**Behaviour:**

A slippery tile that causes Theseus to slide uncontrollably until hitting a
wall or non-ice tile.

1. When Theseus steps onto an ice tile, he **continues sliding** in the same
   direction until he hits a **wall** or reaches a **non-ice tile**.
2. The entire slide happens as **one move** within the Theseus phase — the
   environment phase does not trigger between slide steps.
3. If Theseus slides onto the Minotaur's tile, standard collision rules
   apply (loss).
4. If Theseus slides onto a hazardous tile (e.g. active spike trap), he dies.
5. If Theseus slides onto a non-ice feature tile (e.g. teleporter, pressure
   plate), that feature activates normally at the final landing position.
6. The Minotaur is **not affected** by ice tiles — the Minotaur moves
   normally across ice.

**Config (level JSON):**
- No additional config needed (position only).

#### 3.2.11 Groove Box

| Property    | Value                                |
|-------------|--------------------------------------|
| **Type**    | Tile (entity on groove track)        |
| **Biome**   | Introduced in Stone Labyrinth        |

**Behaviour:**

A heavy box sitting in a fixed groove (track). Theseus can push the box along
its groove. The box blocks movement for both actors.

1. The groove is a linear set of tiles (horizontal or vertical) defining the
   track the box can slide along.
2. When Theseus attempts to move **into** the box tile from a direction
   **aligned with the groove**, the box slides one tile in the push direction
   (if the next groove tile is empty and not walled off).
3. Theseus moves into the tile the box vacated (the push costs Theseus's
   move action).
4. If Theseus pushes from a direction **not aligned with the groove**, the
   move is simply blocked — the box does not move.
5. The box **blocks the Minotaur** — the Minotaur cannot push boxes and
   treats the box tile as impassable.
6. The box **blocks Theseus** if the push would move the box off the groove,
   into a wall, or into another box.

**Config (level JSON):**
- `"groove"`: array of `{col, row}` coordinates defining the groove track
- `"initial_pos"`: `{col, row}` — starting position of the box within the
  groove (must be one of the groove tiles)

### 3.3 Feature Interaction with Actors

- Features that **block movement** behave like walls for both Theseus and the
  Minotaur unless otherwise specified (e.g. manual turnstiles are only
  interactive for Theseus).
- Features that **move actors** (e.g. auto-turnstile, moving platform) apply
  to both Theseus and the Minotaur equally.
- Features that are **deadly** (e.g. spike traps when active, Medusa gaze,
  pit tiles) **kill Theseus** but **never kill the Minotaur**. The Minotaur
  is immune to all lethal hazards.
- Features may **block** the Minotaur's movement (acting as walls), but the
  Minotaur is never killed, frozen, or incapacitated. The Minotaur is always
  alive and always on the board.
- Only Theseus triggers **reactive features** (spike traps, pressure plates,
  manual turnstiles). The Minotaur does not trigger these.

### 3.4 Feature Interaction Summary Table

| Feature          | Theseus Trigger | Minotaur Trigger | Deadly | Blocks Minotaur |
|------------------|-----------------|------------------|--------|-----------------|
| Spike Trap       | Step on/wait    | No               | When up | When up        |
| Pressure Plate   | Step on         | No               | No     | No              |
| Locking Gate     | Pass through    | Pass through     | No     | After locked    |
| Auto-Turnstile   | Automatic       | Automatic        | Indirect| Indirect       |
| Manual Turnstile | Push wall       | No               | No     | No              |
| Teleporter       | Step on         | Step on          | No     | No              |
| Crumbling Floor  | Leave/wait      | No (immune)      | After collapse | After collapse |
| Moving Platform  | Rides           | Rides            | No (pit is deadly) | Pit blocks |
| Medusa Wall      | Move toward     | No               | Yes    | No              |
| Ice Tile         | Step on         | No               | No     | No              |
| Groove Box       | Push along groove| No              | No     | Yes             |

See [01 -- Core Mechanics](01-core-mechanics.md) §2 and §5.2 for full actor
interaction rules.

## 4. Star Rating System

Each level has an **optimal turn count** (the minimum number of turns needed
to solve the puzzle). This value is computed by the external level generator
and stored in the level data.

| Stars | Requirement                                          |
|-------|------------------------------------------------------|
| 0     | Level not yet completed                               |
| 1     | Level completed (any turn count)                      |
| 2     | Level completed in the optimal turn count              |

Stars are used for overworld progression gating -- see
[04 -- Overworld](04-overworld.md) §7.
