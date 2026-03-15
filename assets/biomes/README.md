# assets/biomes/

Biome configuration files for the procedural diorama generator. Each JSON file defines the visual theme for a set of levels.

## Files

| File                    | Purpose |
|------------------------|---------|
| `stone_labyrinth.json` | Default biome. Warm sandstone palette with regular stacked-block walls, subtle floor pebbles and cracks. Cyan-tinted lanterns. |
| `dark_forest.json`     | Forest biome. Dark green palette with rough, irregular walls. Mushroom and twig floor decorations, thick moss and vine wall decorations. Green lanterns. |
| `crystal_caverns.json` | Icy cavern biome. Cool blue-grey palette with smooth walls and subtle frost decorations. Used by ice-tile test levels. Blue-tinted lanterns. |

## JSON Format

Each biome JSON contains:

- `id`, `name` — Identifier and display name
- `palette` — Color arrays for floor (A/B checkerboard), walls, accent, impassable, platform
- `wall_style` — Block composition parameters (blocks per segment, rows, regularity, roughness, mortar gap)
- `floor_decorations`, `wall_decorations` — Density, max per tile, prefab name references
- `lantern_pillars` — Glow color, density, placement rules
- `platform`, `edge_border`, `doors` — Structural parameters
- `floor_shadow` — Shadow lightmap parameters (shadow_scale, shadow_offset_x/z, shadow_blur_radius, shadow_intensity, shadow_resolution)
- `prefabs` — Inline voxel cluster definitions (named, each with an array of boxes)

Levels reference a biome by its `id` in the level JSON's `"biome"` field.
