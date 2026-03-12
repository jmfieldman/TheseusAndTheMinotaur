# 03 -- Level Design

## 1. Biomes

The game contains approximately **12 biomes**, all rooted in Greek mythology,
ancient Crete, and labyrinthine aesthetics.

### 1.1 Candidate Biome List

| #  | Biome                  | Description                                              |
|----|------------------------|----------------------------------------------------------|
| 1  | Stone Labyrinth        | Classic Cretan maze, sandstone and marble                |
| 2  | Dark Forest            | Ancient twisted olive/cypress groves, overgrown ruins    |
| 3  | Mechanical Halls       | Bronze gears, Daedalus-style mechanisms and automata     |
| 4  | Infernal Dungeon       | Deep underground, lava cracks, Tartarus-inspired         |
| 5  | Sunken Ruins           | Flooded temple corridors, shallow water, mossy stone     |
| 6  | Palace of Knossos      | Ornate Minoan palace, painted columns, frescoes          |
| 7  | Catacombs              | Bone-lined corridors, dim torchlight, narrow passages    |
| 8  | Crystal Caverns        | Underground geode chambers, faint luminescent minerals   |
| 9  | Overgrown Temple       | Jungle reclaiming ancient architecture, vines and roots  |
| 10 | Volcanic Forge         | Hephaestus-inspired, molten metal channels, anvils       |
| 11 | Celestial Observatory  | Open-air stargazing platform, astrolabes, constellations |
| 12 | The Minotaur's Sanctum | Final biome, dark and foreboding, culminating encounter  |

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
- Theseus start position
- Minotaur start position
- Exit tile position
- Environmental feature placement and configuration
- Biome identifier (determines visual theme)
- Metadata (level name, difficulty rating, author, etc.)

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

> **TBD:** Features will be designed and added iteratively. Below is a starter
> list of candidates to explore.

| Feature           | Type       | Behavior (Draft)                                    |
|-------------------|------------|-----------------------------------------------------|
| Spike Trap        | Tile       | Cycles between up/down on a fixed turn interval; deadly when up |
| Pressure Plate    | Tile       | Activates/deactivates linked walls when occupied    |
| One-Way Gate      | Edge       | Passable in one direction only                      |
| Rotating Wall     | Edge       | Wall rotates 90 degrees each turn cycle             |
| Conveyor          | Tile       | Pushes any actor one tile in a fixed direction during environment phase |
| Teleporter Pair   | Tile       | Stepping onto one tile moves actor to the paired tile |
| Crumbling Floor   | Tile       | Passable once; becomes a pit after first traversal  |
| Locked Door / Key | Edge+Tile  | Door blocks passage until Theseus collects the key  |

### 3.3 Feature Interaction with Actors

- Features that **block movement** behave like walls for both Theseus and the
  Minotaur.
- Features that **move actors** (e.g. conveyor) apply to both Theseus and the
  Minotaur equally unless otherwise specified.
- Features that are **deadly** (e.g. spike traps when active): **TBD** whether
  they kill Theseus, the Minotaur, or both.

> **Open question:** Can the Minotaur be killed/stunned by environmental
> features? If so, does the player still need to reach the exit, or is
> Minotaur elimination an alternate win condition?
