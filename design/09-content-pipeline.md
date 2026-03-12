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
- Tile type map (walkable vs. impassable environment tiles)
- Actor start positions (Theseus, Minotaur)
- Exit tile position
- Environmental feature definitions (type, position, configuration, initial state)
- Biome identifier
- Level metadata (name, difficulty, id, ordering)

Level data is **purely logical** -- it contains no visual information. All
visual representation (voxel geometry, wall styling, floor decorations,
environmental dressing) is generated procedurally at runtime by the engine
using the biome's procedural generation configuration (see §3).

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
    { "col": 2, "row": 3, "side": "south" }
  ],
  "impassable": [
    { "col": 4, "row": 5 },
    { "col": 4, "row": 6 }
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

### 2.4 Impassable Tiles

Tiles listed in the `"impassable"` array are **environment tiles** that neither
Theseus nor the Minotaur can enter. They function as solid blocking regions
(equivalent to a tile walled on all four sides, but with distinct visual
treatment).

The procedural diorama generator renders impassable tiles as **biome-appropriate
blocking space** rather than empty floor:

| Biome              | Impassable Tile Rendering Examples                |
|--------------------|---------------------------------------------------|
| Dark Forest        | Dense tree trunks, thick undergrowth, fallen logs |
| Sunken Ruins       | Deep water pools, flooded sections                |
| Infernal Dungeon   | Lava pools, magma-filled pits                     |
| Mechanical Halls   | Large bronze machinery, gear assemblies           |
| Crystal Caverns    | Massive crystal formations, stalagmite clusters   |
| Catacombs          | Collapsed rubble, bone piles, cave-ins            |
| Stone Labyrinth    | Solid stone columns, large boulders               |

Impassable tiles give level designers a way to create non-rectangular playable
areas and interesting negative space without the visual monotony of walls on
every edge.

> **Open question:** Should we use a binary/compact format instead of JSON for
> shipping builds, with JSON as the authoring format? A simple compile step
> could convert JSON to a packed binary format.

## 3. Procedural Diorama Generation

Level dioramas are **fully procedurally generated** at runtime from the logical
level data (§2) combined with the biome's procedural generation configuration
(§6). No per-level meshes are hand-authored. This scales to 100--200 levels
without prohibitive art costs.

### 3.1 Generation Pipeline

```
Level JSON (logical data)
    +
Biome procgen config (§6.2)
    │
    ▼
Procedural Diorama Generator (runtime)
    │  1. Lay floor tiles (checkerboard coloring, per-biome palette)
    │  2. Generate walls (biome-styled voxel compositions)
    │  3. Fill impassable tiles (biome-appropriate blocking geometry)
    │  4. Place environmental features (spike traps, pressure plates, etc.)
    │  5. Scatter floor decorations (biome-themed micro-details)
    │  6. Apply wall decorations (moss, cracks, dripping, etc.)
    │  7. Place environmental light sources (lanterns, lava glow, etc.)
    │  8. Place exit tile effects (god-light, finish-flag border)
    │  9. Build surrounding diorama edge / border geometry
    │
    ▼
Voxel Mesh (GPU-ready VBO, vertex colors, per-frame rebuild for dynamic parts)
```

The generator runs once when a level loads. The resulting mesh is static for the
duration of the level (only actors and stateful environmental features animate).

### 3.2 Floor Generation

#### 3.2.1 Checkerboard Grid

Walkable floor tiles use a **subtle checkerboard pattern** -- every other tile
is a slightly offset color from the biome's floor palette. This makes the grid
inherently visible even in areas with no walls, without requiring explicit grid
lines.

- The color offset should be **low-contrast** (e.g. 5--10% lightness shift) --
  enough to perceive the grid at a glance but not visually noisy.
- Both checkerboard colors are derived from the biome's floor palette.

#### 3.2.2 Floor Surface Detail

Floors are **not perfectly flat**. The procedural generator scatters small
voxel details on walkable tiles to add visual interest:

| Biome              | Floor Decoration Examples                          |
|--------------------|----------------------------------------------------|
| Dark Forest        | Small clover patches, fallen leaves, tiny mushrooms|
| Stone Labyrinth    | Scattered pebbles, sand drifts, worn tile edges    |
| Mechanical Halls   | Bolt heads, small gear fragments, oil stains        |
| Catacombs          | Loose pebbles, bone fragments, dust piles          |
| Sunken Ruins       | Shallow puddles, moss patches, cracked stone        |
| Crystal Caverns    | Tiny crystal shards, mineral deposits               |
| Infernal Dungeon   | Ash patches, small embers, scorched marks           |
| Overgrown Temple   | Vine tendrils, root tips, small flowers             |

- Decorations are placed **randomly per tile** using a seeded RNG (seed
  derived from level ID + tile coordinate) so layouts are deterministic and
  reproducible.
- Decorations must not obscure gameplay-critical information (actor positions,
  feature states). They sit at or below floor level and are purely cosmetic.
- Decoration **density and variety** are configurable per biome (see §6.2).

### 3.3 Wall Generation

Walls are **procedurally generated compositions of voxels**, not flat planes.
Walls are **low-profile** (~25--35% of tile width in height) -- tall enough to
clearly read as barriers but short enough to never occlude actors or tile
contents behind them (see [02 -- Visual Style](02-visual-style.md) §2.2).

Each biome defines a wall generation style that controls:

- **Voxel shape variation:** Whether wall voxels are uniform cubes (Mechanical
  Halls) or irregular/offset (Catacombs, Dark Forest).
- **Surface roughness:** How much positional jitter is applied to wall voxels.
  Smooth for palace/mechanical themes, rough for natural/underground themes.
- **Color variation:** Per-voxel color jitter within the biome's wall palette
  range.
- **Height variation:** Whether walls are uniform height or have ragged/
  crumbling tops.
- **Decorative overlays:** Moss growth, cracks, dripping water stains,
  cobwebs, etc. applied as additional voxels on wall surfaces (see §3.5).

| Biome              | Wall Style                                         |
|--------------------|----------------------------------------------------|
| Stone Labyrinth    | Regular sandstone blocks, minor weathering          |
| Dark Forest        | Rough stone with heavy moss/vine growth             |
| Mechanical Halls   | Bronze-toned, aligned, with rivets and panel lines  |
| Catacombs          | Rocky, irregular, crumbling sections, bone inlays   |
| Sunken Ruins       | Waterlogged stone, algae, partial collapse           |
| Crystal Caverns    | Crystalline formations mixed with raw rock           |
| Palace of Knossos  | Ornate painted columns, clean Minoan masonry         |

### 3.4 Impassable Tile Generation

Impassable tiles are filled with **biome-appropriate blocking geometry** that
is visually distinct from walls (see §2.4 for per-biome examples). The
generator selects from a set of prefab voxel clusters defined in the biome's
procgen config, with randomized rotation, scale variation, and color jitter.

### 3.5 Decoration Layers

The procedural generator applies decorations in **modular layers**, each
independently configurable per biome:

| Layer              | Applied To      | Examples                              |
|--------------------|-----------------|---------------------------------------|
| Floor scatter      | Walkable tiles  | Pebbles, leaves, flowers, shards      |
| Wall surface       | Wall faces      | Moss, cracks, cobwebs, water stains   |
| Wall top           | Wall upper edge | Crumbling voxels, small plants, snow  |
| Impassable fill    | Blocked tiles   | Trees, lava, columns, rubble          |
| Edge border        | Diorama perimeter | Cliff edges, water, void, roots     |
| Light sources      | Specific tiles/walls | Lanterns, torches, lava cracks     |

Each layer has:

- **Density** (0.0 -- 1.0): How frequently decorations appear.
- **Variety** (list of prefab voxel clusters): Pool of options to choose from.
- **Placement rules:** Where decorations can appear (e.g. moss only on walls
  adjacent to open tiles, lanterns only on wall ends).
- **RNG seed:** Derived from level ID for deterministic output.

### 3.6 Actor Models

- **Theseus** and **Minotaur** are the only pre-authored assets that are not
  procedurally generated.
- Each actor is a small voxel model with animation states (idle, hop/roll,
  celebrate, caught/death).
- Authored in a voxel editor or modeled directly in code as positioned voxel
  arrays.
- The Minotaur model includes retractable horn and face sub-meshes (see
  [02 -- Visual Style](02-visual-style.md) §2.4 and §7.2).

### 3.7 Mesh Format

Since dioramas are procedurally generated, the engine builds GPU-ready vertex
buffers at runtime rather than loading mesh files for levels. The mesh format
considerations apply only to **actor models** and **decoration prefabs**:

| Format   | Use Case                          | Notes                        |
|----------|-----------------------------------|------------------------------|
| Custom   | Voxel prefabs (decoration clusters) | Simple positioned voxel arrays; can be defined in JSON or binary |
| glTF     | Actor models (if complex animation needed) | Well-supported, binary variant is compact |

> **TBD:** Actor model format. If actors are simple enough to define as
> positioned voxel arrays with tween-based animation, a custom format may be
> simpler than glTF.

### 3.8 Textures

- Minimal texture use -- the procedural generator relies on **vertex colors**
  for all voxel geometry.
- Where needed: small atlases (e.g. 256x256) for subtle surface detail
  (e.g. the exit tile's checkered border pattern).
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
    biome.json         -- palette, feature set, procgen configuration
    overworld.yml      -- overworld graph, nodes, edges, star gates, secrets
    prefabs/           -- decoration voxel cluster prefabs (JSON or binary)
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
    "floor_a": "#4a5240",
    "floor_b": "#455038",
    "wall": "#2b3025",
    "wall_jitter": 0.08,
    "accent": "#6b7a5c",
    "impassable": "#1e2518"
  },
  "features_introduced": ["spike_trap"]
}
```

- **floor_a / floor_b:** The two checkerboard tile colors (see §3.2.1).
- **wall_jitter:** Maximum per-voxel color variation applied to wall voxels
  (fraction of palette range).

### 6.2 Procedural Generation Configuration

The `biome.json` also contains (or references) the procedural generation
parameters that control how the diorama generator builds levels for this biome:

```json
{
  "procgen": {
    "wall_style": {
      "roughness": 0.3,
      "height_variation": 0.1,
      "voxel_shape": "irregular",
      "color_jitter": 0.08
    },
    "floor_decorations": {
      "density": 0.15,
      "prefabs": ["clover_patch", "fallen_leaf", "tiny_mushroom", "pebble"],
      "max_per_tile": 2
    },
    "wall_decorations": {
      "density": 0.25,
      "prefabs": ["moss_patch", "vine_tendril", "crack", "cobweb"],
      "placement": "wall_face"
    },
    "wall_top_decorations": {
      "density": 0.1,
      "prefabs": ["crumble_voxels", "small_fern"],
      "placement": "wall_top"
    },
    "impassable_prefabs": ["dense_trees", "thick_undergrowth", "fallen_log"],
    "edge_border": {
      "style": "cliff_with_roots",
      "depth": 2
    },
    "light_sources": {
      "type": "lantern",
      "placement": "wall_end",
      "density": 0.1
    }
  }
}
```

Each decoration layer is modular and independently tunable. The engine loads
prefab voxel clusters from the `prefabs/` directory and places them according
to the density, placement rules, and seeded RNG described in §3.5.

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

## 10. Localization Strings

Player-facing strings are stored in per-locale JSON files:

```
assets/
  strings/
    en.json       -- English (launch locale)
    fr.json       -- (future)
    de.json       -- (future)
```

### 10.1 String File Format

```json
{
  "title_play": "Play",
  "title_continue": "Continue",
  "title_settings": "Settings",
  "title_quit": "Quit",
  "hud_turns": "Turns: {0}",
  "results_best": "Best: {0}",
  "save_empty": "New Game",
  "gate_locked": "{0} stars required"
}
```

- Keys are stable identifiers used in code.
- Values support simple `{0}`, `{1}` positional placeholders.
- **English only at launch.** Additional locale files can be shipped without
  code changes.
