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

## 5. Biome Definition

Each biome is a data-driven bundle:

```
biomes/
  dark_forest/
    biome.json         -- palette, feature set, overworld graph
    diorama_template/  -- base mesh + decoration meshes
    music/             -- biome soundtrack(s)
    ambient/           -- ambient loop(s)
    sfx/               -- biome-specific SFX overrides
```

### 5.1 biome.json

```json
{
  "id": "dark_forest",
  "name": "Dark Forest",
  "palette": {
    "floor": "#4a5240",
    "wall": "#2b3025",
    "accent": "#6b7a5c"
  },
  "features_introduced": ["spike_trap"],
  "overworld_graph": { ... }
}
```

## 6. Build Pipeline

```
Source Assets (JSON, OBJ/glTF, PNG, OGG, WAV)
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
