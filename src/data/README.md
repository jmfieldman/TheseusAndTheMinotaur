# src/data/

Data persistence and content loading. Handles localization strings, user settings, and save game data.

## Files

| File                | Purpose |
|---------------------|---------|
| `strings.h / strings.c` | Localization string table. Loads `en.json` (key → string map) via cJSON into a hash map (open addressing, 128 buckets). `strings_get(key)` returns the localized string or the key itself as fallback. Supports `{0}`, `{1}` positional placeholders (format not yet implemented). |
| `settings.h / settings.c` | Global settings (not per-save-slot). `Settings` struct: music_volume, sfx_volume, anim_speed (1.0–4.0, fast-forward multiplier for buffered input), fullscreen, resolution, camera_perspective (ortho/perspective toggle), camera_fov (5–90°, vertical FOV for perspective mode). Loads/saves YAML via libyaml. `settings_default()` provides sensible defaults. Stored at `{save_dir}/settings.yml`. |
| `save_data.h / save_data.c` | Per-slot save data. 3 slots (0–2). `SaveSlot` struct tracks play time, current biome/node, and per-biome level progress (completed, best turns, stars). YAML serialization. Helper functions for total stars, completion count, and formatted play time. |

## Data Flow

```
assets/strings/en.json  →  strings_init()  →  strings_get("key")
{save_dir}/settings.yml →  settings_load() →  g_settings struct
{save_dir}/save_N.yml   →  save_data_load()→  SaveSlot struct
```

## File Locations

Settings and saves are stored in `platform_get_save_dir()` (see `src/platform/`).
String tables are loaded from `{asset_dir}/assets/strings/en.json`.
