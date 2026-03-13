# assets/

Runtime assets shipped with the game. Copied to the build directory on each build.

## Directory Structure

| Path                    | Purpose |
|------------------------|---------|
| `fonts/theseus.ttf`    | Game font (currently Inter Regular as placeholder). Used by `text_render.c` at 5 sizes (16, 24, 32, 48, 64). Will be replaced with a thematic font. |
| `strings/en.json`      | English localization string table. Key → display text mapping. Loaded at startup by `strings_init()`. Additional locale files (e.g., `fr.json`) can be added here. |
| `levels/`              | Level JSON files organized by biome. Currently contains `tutorial/` with two test levels for the text-mode puzzle prototype. |
| `biomes/`              | Biome configuration JSON files. Each biome defines palette colors, wall style, decoration prefabs, lantern settings, and other visual parameters. Currently: `stone_labyrinth.json` (warm sandstone) and `dark_forest.json` (dark greens, moss). |

## Future

- `audio/` — Music (OGG), SFX (WAV), ambient (OGG) per biome
- `gamedata.tar.gz` — Packaged level data, biome configs, overworld definitions (built by asset pipeline)
