# src/scene/

Game scenes (states). Each scene implements the `State` vtable interface and is managed by the state manager stack.

## Files

| File                | Purpose |
|---------------------|---------|
| `title_scene.h / title_scene.c` | Main menu. Shows game title and menu items (Play, Continue, Settings, Quit). Continue only appears if a save exists; Quit is hidden on iOS/tvOS. Animated highlight with pulse effect and fade-in on enter. Play pushes SaveSelectScene, Settings pushes SettingsScene. |
| `save_select_scene.h / save_select_scene.c` | Save slot picker. Displays 3 slot cards showing biome name, progress, and play time (or "New Game" for empty slots). Press right on an occupied slot to access delete. Delete confirmation is a sub-mode with a Yes/No dialog. Back pops to title. |
| `settings_scene.h / settings_scene.c` | Settings overlay (transparent — renders states below). Music volume, SFX volume (sliders adjusted with left/right), and Fullscreen toggle (desktop only). Auto-saves settings.yml on exit. |

## Adding a New Scene

1. Create `my_scene.h` with `State* my_scene_create(void);`
2. Implement the `State` vtable in `my_scene.c` (allocate with `calloc`, free in `destroy`)
3. Set `base.transparent = true` if it should render as an overlay
4. Push it with `engine_push_state(my_scene_create())`

## Future Scenes

- **Overworld** — Biome diorama navigation with mini-diorama level nodes
- **Puzzle** — Active puzzle gameplay (zoomed into one diorama)
- **ZoomTransition** — Camera interpolation between overworld and puzzle
- **PauseOverlay** — Pause menu during puzzle gameplay
