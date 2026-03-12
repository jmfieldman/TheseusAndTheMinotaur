# 09 -- Content Pipeline

## 1. Overview

Content for the game comes from two external sources:

1. **Level data** -- produced by a separate level generator tool.
2. **Visual assets** -- voxel diorama meshes, actor models, textures, audio.

This document defines the formats and workflows the engine expects.

## 2. Level Data Format

### 2.1 Requirements

The level data format must encode:

- Grid dimensions (width, height)
- Wall map (per-edge: which edges between adjacent tiles have walls)
- Actor start positions (Theseus, Minotaur)
- Exit tile position
- Environmental feature definitions (type, position, configuration, initial state)
- Biome identifier
- Level metadata (name, difficulty, id, ordering)

### 2.2 Proposed Format

JSON (human-readable, easy to generate and parse):

```json
{
  "id": "forest-03",
  "name": "Tangled Path",
  "biome": "dark_forest",
  "difficulty": 3,
  "optimal_turns": 14,
  "grid": {
    "width": 6,
    "height": 8
  },
  "theseus": { "col": 0, "row": 0 },
  "minotaur": { "col": 5, "row": 7 },
  "exit": { "col": 3, "row": 0 },
  "walls": [
    { "col": 2, "row": 3, "side": "east" },
    { "col": 2, "row": 3, "side": "south" },
    ...
  ],
  "features": [
    {
      "type": "spike_trap",
      "position": { "col": 4, "row": 2 },
      "config": { "cycle": 3, "initial_phase": 0 }
    }
  ]
}
```

### 2.3 Wall Encoding

Each wall is defined by a tile coordinate and the side of that tile:

- `"north"` -- wall on the top edge of the tile
- `"south"` -- wall on the bottom edge
- `"east"` -- wall on the right edge
- `"west"` -- wall on the left edge

Shared edges need only be specified once (e.g. a wall between (2,3) east and
(3,3) west is stored as one entry).

> **Open question:** Should we use a binary/compact format instead of JSON for
> shipping builds, with JSON as the authoring format? A simple compile step
> could convert JSON to a packed binary format.

## 3. Visual Assets

### 3.1 Diorama Meshes

Each level or biome needs a 3D diorama mesh that the puzzle grid maps onto.

**Authoring options:**

- **Hand-modeled** in a voxel editor (MagicaVoxel, Goxel) or DCC tool
  (Blender).
- **Procedurally generated** from the level grid data + biome decorations.
- **Template + variation** -- a biome-specific template diorama with
  procedural placement of walls, floors, and decoration voxels based on level
  data.

> **TBD:** Which approach? Template + variation is likely the most practical
> for 100--200 levels -- hand-modeling each would be prohibitive.

### 3.2 Mesh Format

The engine needs to load meshes at runtime. Options:

| Format   | Pros                              | Cons                         |
|----------|-----------------------------------|------------------------------|
| OBJ      | Simple, universal                 | No animation, verbose        |
| glTF     | Modern, supports animation, compact | More complex to parse       |
| Custom   | Minimal, tailored to our needs    | Must build tooling           |

> **TBD:** glTF is the recommended starting point (well-supported, binary
> variant is compact, animation support for actors).

### 3.3 Actor Models

- Theseus and Minotaur need simple animated models (idle, walk, celebrate,
  caught).
- Low-poly, consistent with the diorama aesthetic.
- Authored in Blender or similar, exported to engine format.

### 3.4 Textures

- Minimal texture use (mostly vertex colors).
- Where needed: small atlases (e.g. 256x256) for subtle surface detail.
- Format: PNG for authoring, engine may convert to a GPU-compressed format at
  build time.

## 4. Audio Assets

| Type     | Format     | Notes                              |
|----------|------------|------------------------------------|
| Music    | OGG Vorbis | Streaming playback, per-biome      |
| SFX      | WAV        | Short clips, loaded into memory    |
| Ambient  | OGG Vorbis | Looping background, per-biome      |

## 5. Font Assets

- Text rendering via **SDL_ttf** (TrueType font rendering).
- Ship one or more `.ttf` font files with the game.
- Font style should complement the matte/ancient aesthetic (clean sans-serif
  or a tasteful serif with good readability at small sizes).
- Multiple font sizes may be needed (HUD, menus, titles, level results).
- SDL_ttf renders glyphs to SDL surfaces which are then uploaded as OpenGL
  textures for rendering.

> **TBD:** Specific font selection. Must be licensed for commercial game use.

## 6. Biome Definition

Each biome is a data-driven bundle:

```
biomes/
  dark_forest/
    biome.json         -- palette, feature set
    overworld.yml      -- overworld graph, nodes, edges, star gates, secrets
    diorama_template/  -- base mesh + decoration meshes
    music/             -- biome soundtrack(s)
    ambient/           -- ambient loop(s)
    sfx/               -- biome-specific SFX overrides
```

### 6.1 biome.json

```json
{
  "id": "dark_forest",
  "name": "Dark Forest",
  "palette": {
    "floor": "#4a5240",
    "wall": "#2b3025",
    "accent": "#6b7a5c"
  },
  "features_introduced": ["spike_trap"]
}
```

## 7. Overworld Data Format (YAML)

Each biome has an `overworld.yml` that defines the graph structure, node
positions, path waypoints, star gates, and secret conditions.

### 7.1 Grid-Based Coordinate System

Overworld node positions use a **logical grid coordinate system** rather than
pixel coordinates. Each biome's overworld diorama has a defined grid size (e.g.
8x6), and nodes are placed at integer grid positions.

The engine maps grid coordinates to world-space diorama positions at runtime
using a per-biome grid-to-world transform. This keeps the authoring data clean
and resolution-independent.

Path waypoints between nodes also use grid coordinates. The engine interpolates
smooth curves through waypoints for the visual path.

### 7.2 Example

```yaml
biome: dark_forest
grid_size: { cols: 8, rows: 6 }
start_node: df-01

nodes:
  - id: df-01
    type: level
    level_id: dark-forest-01
    position: { col: 1, row: 5 }
    exits:
      north: df-02

  - id: df-02
    type: level
    level_id: dark-forest-02
    position: { col: 1, row: 3 }
    exits:
      south: df-01
      east: df-03
      north: df-gate-01

  - id: df-gate-01
    type: star_gate
    star_threshold: 6
    position: { col: 1, row: 1 }
    exits:
      south: df-02
      north: df-04

  - id: df-secret-01
    type: secret
    level_id: dark-forest-secret-01
    position: { col: 5, row: 3 }
    reveal_condition:
      type: biome_stars
      threshold: 16
    exits:
      west: df-02

  - id: df-exit
    type: transition
    target_biome: mechanical_halls
    target_node: mh-01
    position: { col: 1, row: 0 }
    exits:
      south: df-10

paths:
  - from: df-01
    to: df-02
    waypoints:
      - { col: 1, row: 4 }
      - { col: 0, row: 3 }
      - { col: 1, row: 3 }
```

### 7.3 Key Fields

- **grid_size:** Logical grid dimensions for this biome's overworld diorama.
  Nodes are placed at integer coordinates within this grid.
- **nodes[].exits:** Map of cardinal direction → target node ID. At most one
  exit per direction.
- **nodes[].position:** Logical grid coordinate (col, row). Mapped to
  world-space by the engine at runtime.
- **paths[].waypoints:** Intermediate grid points for the visual path between
  nodes. The engine interpolates smooth curves through these points. If
  omitted, a straight line is used.
- **star_gate.star_threshold:** Number of biome-local stars required to pass.
- **secret.reveal_condition:** Trigger for revealing the secret node (see
  [04 -- Overworld](04-overworld.md) §8).

## 8. Save File Format (YAML)

Save files use YAML for human readability and debugging. One file per save
slot.

```yaml
slot: 1
version: 1
play_time_seconds: 7234
current_biome: dark_forest
current_node: df-05

biomes:
  ship_of_theseus:
    unlocked: true
    levels:
      - id: tutorial-01
        completed: true
        best_turns: 3
        stars: 2
      - id: tutorial-02
        completed: true
        best_turns: 5
        stars: 1
      - id: tutorial-03
        completed: true
        best_turns: 8
        stars: 2
    secrets_revealed: []

  stone_labyrinth:
    unlocked: true
    levels:
      - id: stone-01
        completed: true
        best_turns: 12
        stars: 1
      # ...
    secrets_revealed: [sl-secret-01]

  dark_forest:
    unlocked: true
    levels:
      - id: dark-forest-01
        completed: true
        best_turns: 14
        stars: 2
      - id: dark-forest-02
        completed: true
        best_turns: 9
        stars: 1
      - id: dark-forest-03
        completed: false
      # ...
    secrets_revealed: []

```

### 8.1 Global Settings File

Settings are stored in a **separate global file** (`settings.yml`), not per
save slot. This ensures settings persist across all save slots and are not lost
when a slot is deleted.

```yaml
version: 1
music_volume: 0.8
sfx_volume: 1.0
fullscreen: true
resolution: { width: 1920, height: 1080 }
```

Settings file is stored alongside save files in the platform-specific save
directory (see [10 -- Platform Targets](10-platform-targets.md) §3.1).

### 8.2 Save File Locations

See [10 -- Platform Targets](10-platform-targets.md) §3.1 for platform-specific
save paths.

### 8.3 Save Behavior

- Auto-save on level completion and biome transition.
- No mid-level save.
- Save format version field for future migration support.

## 9. Build Pipeline

```
Source Assets (JSON, YAML, OBJ/glTF, PNG, OGG, WAV, TTF)
    │
    ▼
Asset Compiler (build step)
    │  • Validates level JSON
    │  • Converts meshes to engine-optimized format
    │  • Compresses textures
    │  • Packages into biome bundles
    │
    ▼
Runtime Assets (binary bundles loaded by engine)
```

> **TBD:** Asset compiler tooling needs to be built. Could be a simple
> Python/shell script pipeline or a more structured tool.
