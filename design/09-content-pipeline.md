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
- **Entrance door** (boundary wall segment: position and side)
- **Exit door** (boundary wall segment: position and side)
- Actor start positions (Theseus start = tile adjacent to entrance; Minotaur)
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
  "entrance": { "col": 0, "row": 3, "side": "west" },
  "exit": { "col": 5, "row": 4, "side": "east" },
  "theseus": { "col": 0, "row": 3 },
  "minotaur": { "col": 5, "row": 7 },
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

### 2.3a Entrance and Exit Door Encoding

Each level has an `"entrance"` and `"exit"` object specifying boundary wall
openings:

```json
"entrance": { "col": 0, "row": 3, "side": "west" },
"exit": { "col": 5, "row": 4, "side": "east" }
```

- `col` / `row`: The grid tile adjacent to the door opening (the tile
  Theseus enters onto, or the last interior tile before the exit).
- `side`: Which boundary wall the door is on. Must be a boundary edge:
  `"west"` (left), `"east"` (right), or `"north"` (top/back).
  `"south"` (bottom/camera-facing) is **not allowed**.
- The `"theseus"` start position must match the entrance tile (the tile
  specified in the entrance object).
- The entrance and exit must be on **different** wall segments.

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

Level data files remain in **JSON format** for both authoring and shipping.
All static data files (level JSON, biome configs, localization strings, procgen
configs) are packaged into a single **`gamedata.tar.gz`** archive that is
shipped with the game and decompressed into memory at startup. The total data
volume is small enough that this is fast and memory-efficient. See §9 for
packaging details.

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
    │  1. Build diorama platform (raised base with biome-styled side faces)
    │  2. Lay floor tiles (checkerboard coloring, per-biome palette)
    │  3. Generate walls (stacked-block voxel compositions with archways)
    │  3a. Generate entrance/exit doors (boundary openings, virtual exit tile)
    │  4. Fill impassable tiles (biome-appropriate blocking geometry)
    │  5. Place environmental features (spike traps, pressure plates, etc.)
    │  6. Scatter floor decorations (biome-themed micro-details)
    │  7. Apply wall decorations (moss, cracks, dripping, etc.)
    │  8. Place lantern pillars (tall columns with glowing tops at edges/corners)
    │  9. Place exit door effects (warm god-light through opening, finish-flag border)
    │ 10. Build surrounding edge / border geometry (foliage, cliffs, etc.)
    │
    ▼
Voxel Mesh (GPU-ready VBO, vertex colors, per-frame rebuild for dynamic parts)
```

The generator runs once when a level loads. The resulting mesh is static for the
duration of the level (only actors and stateful environmental features animate).

### 3.2 Floor Generation

#### 3.2.1 Checkerboard Grid

Walkable floor tiles use a **checkerboard pattern** -- every other tile uses an
offset color from the biome's floor palette. This makes the grid inherently
visible even in areas with no walls, without requiring explicit grid lines.

- The color offset should be **~5--15% lightness shift** -- enough to clearly
  perceive the grid at a glance but not visually garish.
- Both checkerboard colors are derived from the biome's floor palette.
- Each floor tile is composed of **multiple smaller voxel blocks** (paving
  stones, flagstones, planks depending on biome) with subtle per-block color
  variation. This gives floors visual richness while preserving the
  tile-level checkerboard readability.

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

- **Block composition:** Walls are visibly composed of **individual stacked
  stone/brick voxel blocks** with mortar gaps between them. The block size,
  regularity, and arrangement vary per biome.
- **Voxel shape variation:** Whether wall blocks are uniform rectangles
  (Mechanical Halls, Palace) or irregular/offset (Catacombs, Dark Forest).
- **Surface roughness:** How much positional jitter is applied to wall voxels.
  Smooth for palace/mechanical themes, rough for natural/underground themes.
- **Color variation:** Per-block color jitter within the biome's wall palette
  range. Each block should be a slightly different shade.
- **Height variation:** Whether walls are uniform height or have ragged/
  crumbling tops.
- **Passage archways:** Where a gap exists between wall segments (i.e. two
  adjacent tiles with no wall between them, flanked by walls on neighboring
  edges), the wall ends curve or step inward to form subtle **archway shapes**
  that frame the passage opening.
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

### 3.3a Entrance and Exit Door Generation

The procedural generator creates two **boundary wall openings** per level
based on the `entrance` and `exit` fields in the level data (see §2.3a).

**Entrance door:**

- A gap in the boundary wall at the specified position and side.
- The opening is framed with biome-themed **door geometry** (stone archway,
  iron gate frame, wooden doorframe, etc.) that is part of the static mesh.
- A **locking mechanism** is rendered as a separate dynamic mesh element
  (stone slab, iron bars, vine growth, portcullis, etc.) that animates
  shut after Theseus enters. The lock style is defined per biome.
- After locking, the mechanism becomes visually solid and reads as part of
  the wall.

**Exit door:**

- A gap in the boundary wall at the specified position and side.
- The opening is framed with similar biome-themed door geometry.
- A **virtual exit tile** extends one tile outward from the grid through the
  opening. This tile is generated as part of the static floor mesh but sits
  outside the logical grid boundary.
- The exit door opening receives the **god-light effect** (warm golden light
  shining through the opening) and the **finish-flag checkered border**.
- If the exit is on the **north wall**, the boundary wall geometry around
  the opening is handled like any other side.

| Biome              | Door Style                                         |
|--------------------|----------------------------------------------------|
| Stone Labyrinth    | Stone archway, sliding stone slab lock             |
| Dark Forest        | Rough timber frame, vine/root growth lock          |
| Mechanical Halls   | Bronze-riveted frame, mechanical portcullis lock   |
| Catacombs          | Rough stone opening, iron bar drop lock            |
| Sunken Ruins       | Waterlogged stone arch, rising water seal lock     |
| Crystal Caverns    | Crystal-framed opening, crystal growth lock        |
| Palace of Knossos  | Ornate Minoan doorframe, painted door panel lock   |

### 3.4 Impassable Tile Generation

Impassable tiles are filled with **biome-appropriate blocking geometry** that
is visually distinct from walls (see §2.4 for per-biome examples). The
generator selects from a set of prefab voxel clusters defined in the biome's
procgen config, with randomized rotation, scale variation, and color jitter.

### 3.5 Decoration Layers

The procedural generator applies decorations in **modular layers**, each
independently configurable per biome:

| Layer              | Applied To           | Examples                              |
|--------------------|----------------------|---------------------------------------|
| Floor scatter      | Walkable tiles       | Pebbles, leaves, flowers, shards      |
| Wall surface       | Wall faces           | Moss, cracks, cobwebs, water stains   |
| Wall top           | Wall upper edge      | Crumbling voxels, small plants, snow  |
| Impassable fill    | Blocked tiles        | Trees, lava, columns, rubble          |
| Lantern pillars    | Diorama edges/corners| Tall columns with glowing crystal top |
| Diorama platform   | Entire grid base     | Raised platform with side faces       |
| Edge border        | Diorama perimeter    | Cliff edges, foliage, water, void     |
| Environmental features | Feature tiles   | Pressure plates, spike traps, etc.    |

Each layer has:

- **Density** (0.0 -- 1.0): How frequently decorations appear.
- **Variety** (list of prefab voxel clusters): Pool of options to choose from.
- **Placement rules:** Where decorations can appear (e.g. moss only on walls
  adjacent to open tiles, lanterns at diorama corners and wall endpoints).
- **RNG seed:** Derived from level ID for deterministic output.

### 3.5a Lantern Pillars

Lantern pillars are the primary decorative light source and a defining visual
element of the diorama aesthetic:

- **Structure:** Tall voxel columns (~actor height, significantly taller than
  walls) with a biome-styled base, shaft, and a **glowing crystal or flame
  element** at the top.
- **Placement:** At diorama corners, along the diorama edge, and at wall
  endpoints. The procgen config controls density.
- **Light emission:** Each lantern emits a **cool-toned point light**
  (cyan/blue/teal) that illuminates nearby floor and wall geometry and casts
  dynamic shadows (see [02 -- Visual Style](02-visual-style.md) §4.3).
- **Biome variation:** The pillar style varies per biome (rough stone columns
  for Stone Labyrinth, ornate bronze columns for Mechanical Halls, bone
  columns for Catacombs, etc.) but all share the tall-column-with-glowing-top
  silhouette.

### 3.5b Diorama Platform and Edge

The playable grid sits on a **raised platform** that gives the diorama its
tabletop miniature feel:

- **Platform:** The floor grid is elevated above the surrounding space. The
  sides of the platform are visible (biome-styled stone, earth, or metal
  depending on biome) and add visual weight.
- **Edge drop-off:** Beyond the platform edge, the diorama transitions into
  biome-appropriate border geometry:
  - **Dark Forest:** Dense foliage, undergrowth, and tree canopy wrapping the
    edges.
  - **Stone Labyrinth:** Clean stone frame or cliff face.
  - **Sunken Ruins:** Water surface surrounding the platform.
  - **Catacombs:** Rough rock faces descending into darkness.
  - **Crystal Caverns:** Raw crystal formations and cave walls.
- The border treatment should make the diorama feel like a **self-contained
  island** -- a miniature world the player is peering into.

### 3.6 Actor Models

Actors are **procedurally generated** in code — no external mesh files are
loaded. Both Theseus and the Minotaur are fundamentally **cubes** (with
beveled edges), built as positioned voxel arrays at runtime:

- **Theseus:** A beveled cube with blue vertex colors — RGB(80, 168, 251). The cube
  geometry is generated once at startup. Animation (hop arc, squash, lean)
  is purely tween-based transforms applied to the cube mesh.
- **Minotaur:** A larger true cube with red vertex colors — RGB(239, 34, 34).
  Includes procedurally generated sub-meshes for:
  - **Horns:** White voxel horn shapes attached to the top face, with
    retract/extend tween support (see [02 -- Visual Style](02-visual-style.md)
    §7.2).
- Animation states (idle, hop/roll, celebrate, caught/death) are all
  tween-based transforms — no skeletal animation or keyframe data needed.

### 3.7 Mesh Format

**Everything is procedurally generated.** The engine builds all GPU-ready
vertex buffers at runtime. There are **no external mesh files** for any
game content — dioramas, actors, decorations, and overworld geometry are
all constructed in code.

The mesh builder uses **freeform box placement** — each box (axis-aligned
cuboid) has an arbitrary position and arbitrary dimensions. Boxes are not
snapped to any voxel grid. This allows artistic control: walls composed of
irregularly sized stone blocks with jitter, thin grass slivers just above
the floor, tiny pebbles at arbitrary positions, etc.

After all boxes are placed, the builder rasterizes them into a **coarse
occupancy grid** (~8 subdivisions per game tile) used for:

1. **Hidden face culling** — skip faces fully occluded by neighboring geometry
2. **Raytraced AO atlas** — for complex geometry (decorations, door frames,
   lanterns), per-texel hemisphere rays are cast against the occupancy grid
   to produce an AO texture atlas

The occupancy grid is discarded after mesh construction. AO uses a three-tier
system: floor faces sample a projected shadow lightmap (configurable per biome),
wall faces use vertex-color heuristic darkening, and complex geometry uses the
raytraced AO atlas. The final vertex format is: position + normal + color + UV
+ ao_mode (13 floats). See [02 -- Visual Style](02-visual-style.md) §4.1 for
details.

Decoration prefabs (voxel clusters for floor scatter, wall decorations,
lantern pillars, etc.) are defined as **positioned box arrays** in the
biome's procgen configuration (JSON). The engine interprets these definitions
and generates vertex data at level load time.

### 3.8 Textures

- Minimal texture use -- the procedural generator relies on **vertex colors**
  for all voxel geometry.
- Where needed: small atlases (e.g. 256x256) for subtle surface detail
  (e.g. the exit door's checkered finish-flag border pattern).
- Format: PNG for authoring, engine may convert to a GPU-compressed format at
  build time.

### 3.9 LOD Mesh Generation (Overworld)

Each puzzle also generates a **low-detail (LOD) mesh** for display on the
overworld as a mini-diorama. All ~10 LOD meshes for a biome are generated
when the biome loads and held in VRAM for the duration of the biome.

The LOD generator runs the same pipeline as the full-detail generator (§3.1)
but with the following simplifications:

| Pipeline Step       | Full Detail                              | LOD                                     |
|---------------------|------------------------------------------|-----------------------------------------|
| Floor tiles         | Checkerboard, per-block paving stones    | Flat checkerboard (single quad per tile)|
| Walls               | Stacked blocks, mortar gaps, variation   | Simplified block silhouette             |
| Back wall           | Full biome theming, decorations          | Flat silhouette with biome color        |
| Entrance/exit doors | Full door frame, lock mechanism          | Simple opening in wall                  |
| Impassable tiles    | Detailed prefab clusters                 | Simplified fill geometry                |
| Floor scatter       | Full decoration layer                    | **Omitted**                             |
| Wall surface decor  | Moss, cracks, cobwebs                    | **Omitted**                             |
| Wall top decor      | Crumble, plants, snow                    | **Omitted**                             |
| Lantern pillars     | Full pillar + glow                       | Simplified pillar + glow point          |
| Exit door god-light | Full volumetric cone                     | Simple glow quad                        |
| Edge border         | Full biome border geometry               | **Omitted** (platform edge is visible)  |
| Diorama platform    | Full platform with side faces            | Simplified platform block               |

The LOD mesh uses the same seeded RNG as the full mesh, so the wall layout,
floor pattern, and overall silhouette are identical -- only detail density
differs. This ensures the zoom transition from LOD to full-detail feels
seamless.

## 4. Audio Assets

| Type     | Format     | Notes                              |
|----------|------------|------------------------------------|
| Music    | OGG Vorbis | Streaming playback, per-biome      |
| SFX      | WAV        | Short clips, loaded into memory    |
| Ambient  | OGG Vorbis | Looping background, per-biome      |

## 5. Font Assets

- Text rendering via **SDL_ttf** (TrueType font rendering).
- A single font file **`theseus.ttf`** is shipped with the game, located at:
  ```
  assets/fonts/theseus.ttf
  ```
- Font style should complement the matte/ancient aesthetic (clean sans-serif
  or a tasteful serif with good readability at small sizes).
- Multiple font sizes are rendered from this single TTF at runtime (HUD,
  menus, titles, level results).
- SDL_ttf renders glyphs to SDL surfaces which are then uploaded as OpenGL
  textures for rendering.

## 6. Biome Definition

Each biome is a data-driven bundle:

```
biomes/
  dark_forest/
    biome.json              -- palette, feature set, procgen configuration
    overworld.yml           -- overworld graph, nodes, edges, star gates, secrets
    overworld_scenery.yml   -- overworld scenery tile placement (per-biome)
    prefabs/                -- decoration voxel cluster prefabs (JSON or binary)
    music/                  -- biome soundtrack(s)
    ambient/                -- ambient loop(s)
    sfx/                    -- biome-specific SFX overrides

scenery_library.yml         -- shared library of terrain types, features, and models
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
  "features_introduced": ["spike_trap", "crumbling_floor"]
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
      "block_size": "medium",
      "block_regularity": 0.4,
      "roughness": 0.3,
      "height_variation": 0.1,
      "color_jitter": 0.08,
      "archway_style": "curved"
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
    "doors": {
      "frame_style": "rough_timber",
      "lock_style": "vine_growth",
      "exit_frame_style": "rough_timber"
    },
    "lantern_pillars": {
      "style": "rough_stone",
      "glow_color": "#66cccc",
      "placement": ["corners", "wall_endpoints"],
      "density": 0.3
    },
    "platform": {
      "elevation": 3,
      "side_style": "rough_stone"
    },
    "edge_border": {
      "style": "dense_foliage",
      "depth": 2
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

## 7.4 Overworld Scenery Library

The **scenery library** (`scenery_library.yml`, stored at the top level of the
biomes directory) defines all reusable terrain types, terrain features, and
model pieces available for overworld map authoring. This is a shared library
referenced by every biome's `overworld_scenery.yml`.

The library contains three categories:

#### 7.4.1 Terrain Types

**One per tile.** Defines the ground type for each overworld grid cell.

```yaml
terrain_types:
  - id: grass
    description: Standard grass ground
    procgen:
      base_color: "#4a5240"
      color_variation: 0.05
      surface: grass_blades

  - id: dirt_path
    description: Worn dirt walking path
    procgen:
      base_color: "#8b7355"
      color_variation: 0.03
      surface: packed_earth

  - id: shallow_water
    description: Shallow coastal water
    animated: true
    procgen:
      base_color: "#2a4a6a"
      wave_speed: 0.5
      transparency: 0.3

  - id: sand
    description: Sandy beach ground
    procgen:
      base_color: "#c2b280"
      color_variation: 0.04
      surface: sand_grains

  - id: stone_floor
    description: Paved stone tiles
    procgen:
      base_color: "#7a7a72"
      color_variation: 0.06
      surface: flagstone
```

Terrain types with `animated: true` are rendered as live 3D tiles on top of
the backdrop (not baked into the pre-rendered image). See §7.5.1.

#### 7.4.2 Terrain Features

Multi-tile or overlaid features that sit on top of terrain. These represent
natural or environmental formations.

```yaml
terrain_features:
  - id: forest_dense
    description: Dense tree canopy coverage
    procgen:
      tree_density: 0.8
      tree_height_range: [3.0, 5.0]
      canopy_spread: 1.2
      variants: [pine_tall, pine_short, oak, dead_tree]
      count_range: [3, 6]
      position_jitter: 0.35
      scale_range: [0.8, 1.2]
      rotation_random: true
      edge_thinning: 0.6

  - id: forest_sparse
    description: Light tree coverage
    procgen:
      tree_density: 0.3
      tree_height_range: [2.5, 4.0]
      variants: [pine_tall, oak, stump]
      count_range: [1, 3]
      position_jitter: 0.4
      scale_range: [0.7, 1.1]
      rotation_random: true
      edge_thinning: 0.8

  - id: river
    description: Flowing river segment
    animated: true
    procgen:
      width: 0.6
      flow_speed: 0.4
      bank_style: natural

  - id: mountain
    description: Rocky mountain formation
    procgen:
      peak_height: 6.0
      roughness: 0.5
      snow_line: 4.0

  - id: lava_flow
    description: Flowing lava channel
    animated: true
    procgen:
      width: 0.4
      glow_color: "#ff4400"
      flow_speed: 0.2
```

#### 7.4.3 Models

Individual placed objects — specific things like buildings, bridges, statues,
or landmarks.

```yaml
models:
  - id: stone_bridge
    description: Small arched stone bridge
    size: { cols: 1, rows: 2 }
    procgen:
      style: arched
      material: sandstone

  - id: ruined_house
    description: Crumbling stone cottage
    size: { cols: 1, rows: 1 }
    procgen:
      wall_height: 2.0
      decay: 0.6

  - id: statue_hero
    description: Heroic statue on pedestal
    size: { cols: 1, rows: 1 }
    procgen:
      pedestal_height: 1.5
      style: greek

  - id: wooden_dock
    description: Small wooden pier
    size: { cols: 1, rows: 3 }
    procgen:
      plank_style: weathered

  - id: portal_arch
    description: Ornate stone archway (biome transition)
    size: { cols: 1, rows: 1 }
    procgen:
      arch_height: 4.0
      glow_color: "#8866ff"
```

Each library entry's `procgen` block defines parameters consumed by the
overworld diorama generator to produce voxel geometry at runtime. The
`animated` flag on terrain types and features indicates tiles that must be
rendered live rather than baked into the backdrop (see §7.5.1).

#### 7.4.4 Per-Tile Seeded Variation

To keep overworld maps visually interesting without requiring hand-placement of
every detail, the overworld generator applies **per-tile seeded variation** to
terrain types and features. Each tile's geometry is generated using a
deterministic seed derived from `hash(biome_id, col, row)`, so the same map
always produces the same output but every tile looks unique.

Variation is controlled by parameters in each library entry's `procgen` block.
The generator applies these variation axes:

- **Color jitter** — Small per-tile random offset to the base color (controlled
  by the existing `color_variation` parameter). Each tile's ground color shifts
  by up to ±`color_variation` in HSV space. Individual sub-elements (paving
  stones, tree trunks) get an additional smaller jitter on top.
- **Sub-element scatter** — Features that contain multiple sub-elements (trees
  in a forest, rocks on a mountain, grass blades) randomly select from the
  entry's `variants` list and scatter them within the tile with position jitter,
  random Y-rotation, and scale variation. Parameters:
  - `variants` — List of sub-element model IDs to randomly select from.
  - `count_range` — `[min, max]` number of sub-elements per tile.
  - `position_jitter` — Maximum XZ offset from grid center (0.0–0.5).
  - `scale_range` — `[min, max]` uniform scale multiplier.
  - `rotation_random` — If true, apply random Y-axis rotation per sub-element.
- **Edge awareness** — Optional `edge_thinning` parameter (0.0–1.0). When set,
  tiles adjacent to a different terrain type reduce their sub-element count and
  height, creating natural-looking transitions (e.g., forest treeline thins at
  the edge of a clearing). The generator checks the 4 cardinal neighbors; each
  non-matching neighbor reduces density by `edge_thinning * 0.25`.
- **Height variation** — For terrain features with vertical extent (mountains,
  trees), the `height_range` parameter specifies `[min, max]` and the per-tile
  seed selects a value within that range.

Example of a terrain feature with full variation params:

```yaml
  - id: forest_dense
    description: Dense tree canopy coverage
    procgen:
      tree_density: 0.8
      tree_height_range: [3.0, 5.0]
      canopy_spread: 1.2
      variants: [pine_tall, pine_short, oak, dead_tree]
      count_range: [3, 6]
      position_jitter: 0.35
      scale_range: [0.8, 1.2]
      rotation_random: true
      edge_thinning: 0.6
```

The scenery editor (§7.6) paints "this tile is `forest_dense`" — the generator
handles making each forest tile visually distinct. This keeps authoring simple
(paint broad regions) while producing rich, varied output.

### 7.5 Overworld Scenery File (Per-Biome)

Each biome has an `overworld_scenery.yml` that defines the explicit placement
of all scenery elements on the overworld grid. This is a separate file from
`overworld.yml` (which defines the navigation graph) for size management and
to allow independent editing with the scenery editor tool (see §7.6).

The scenery file uses the same grid coordinate system and `grid_size` as the
biome's `overworld.yml`.

```yaml
biome: dark_forest
grid_size: { cols: 8, rows: 6 }

# Terrain layer: one entry per grid cell (defines ground type)
terrain:
  - { col: 0, row: 0, type: grass }
  - { col: 1, row: 0, type: grass }
  - { col: 2, row: 0, type: shallow_water }
  - { col: 3, row: 0, type: dirt_path }
  - { col: 0, row: 1, type: grass }
  - { col: 1, row: 1, type: dirt_path }
  # ... one entry per cell in the grid

# Terrain features: overlaid on terrain tiles
features:
  - type: forest_dense
    positions:
      - { col: 3, row: 0 }
      - { col: 3, row: 1 }
      - { col: 4, row: 0 }
  - type: river
    waypoints:
      - { col: 0, row: 2 }
      - { col: 1, row: 2 }
      - { col: 2, row: 3 }
  - type: mountain
    positions:
      - { col: 6, row: 0 }
      - { col: 7, row: 0 }
      - { col: 6, row: 1 }

# Models: individually placed objects
models:
  - type: stone_bridge
    position: { col: 2, row: 3 }
    rotation: 90
  - type: ruined_house
    position: { col: 5, row: 1 }
  - type: portal_arch
    position: { col: 1, row: 0 }
```

#### 7.5.1 Animated Tiles

Tiles that contain animated content (flowing water, lava, animated texture
effects) are **not baked** into the pre-rendered overworld backdrop. Instead,
they are rendered as **live 3D geometry** every frame on top of the backdrop
image, in the same layer as other live overlay elements (Theseus token, node
state indicators, etc.).

A tile is marked as animated when:

- Its **terrain type** has `animated: true` in the scenery library.
- It contains a **terrain feature** with `animated: true`.

The engine collects all animated tile positions at biome load time and renders
them as live geometry. The backdrop texture has these tile regions left empty
(or filled with a neutral base color) so the live tiles composite cleanly on
top. Animation is achieved via **sprite sheet cycling** — each animated tile
type defines a sequence of pre-rendered frames that the engine cycles through
at a configured frame rate. No 3D mesh animation or procedural shader effects
are used for scenery animation.

This keeps the vast majority of the overworld as a cheap static backdrop image
while allowing localized motion for water, lava, and similar effects.

### 7.6 Scenery Editor Tool

An interactive utility (separate from the game engine) for authoring overworld
scenery. The tool provides:

1. **Library editor** — Create and modify terrain types, terrain features, and
   models in `scenery_library.yml`. Preview the procedurally generated voxel
   geometry for each entry in real-time.
2. **Scenery painter** — Load a biome's `overworld_scenery.yml` and
   interactively paint terrain types, place terrain features, and position
   models on the grid. Changes are saved back to the YAML file. The overworld
   graph from `overworld.yml` can be overlaid as a reference layer showing
   node positions and paths.
3. **Preview** — Render the full overworld diorama (scenery + LOD
   mini-dioramas + graph overlay) to verify the visual result before running
   the game.

The scenery editor is needed **before serious overworld map work begins**,
since the overworld rendering pipeline depends on having models and terrain
defined in the library.

The scenery editor is a **standalone application** (`tools/scenery_editor/`)
that reuses the game's rendering and data-loading code but has its own UI layer.
It does not ship with the game. See [11 -- Implementation Plan](11-implementation-plan.md)
Step 11.0 for full implementation details.

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
Source Assets (JSON, YAML, OGG, WAV, TTF)
    │
    ▼
Asset Packager (build step)
    │  • Validates level JSON (schema, wall consistency, door placement)
    │  • Validates biome configs (palette ranges, procgen params)
    │  • Validates scenery files (library references, grid bounds)
    │  • Packages all static data into gamedata.tar.gz:
    │      - Level JSON files (per-biome directories)
    │      - Biome configs (biome.json, procgen config)
    │      - Overworld definitions (overworld.yml)
    │      - Overworld scenery (overworld_scenery.yml per biome)
    │      - Scenery library (scenery_library.yml)
    │      - Localization strings (strings/*.json)
    │  • Font file (theseus.ttf) shipped separately in assets/fonts/
    │  • Audio files (OGG, WAV) shipped separately in assets/audio/
    │
    ▼
Runtime Loading:
    1. Engine reads gamedata.tar.gz → decompresses into memory
    2. Parses JSON/YAML into runtime data structures
    3. Font and audio loaded from their asset directories
    4. No mesh files to load — all geometry is procedurally generated
```

The asset packager is a simple build script (Python or shell) that validates
and archives. Since there are no external mesh files and all geometry is
procedural, the pipeline is lightweight.

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
