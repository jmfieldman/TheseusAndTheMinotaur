# src/

Game engine source code. Organized into subsystem directories with strict dependency rules:

| Directory    | Purpose                                              | Dependencies          |
|-------------|------------------------------------------------------|-----------------------|
| `engine/`   | Core engine: game loop, state manager, utilities     | SDL3, glad            |
| `input/`    | Input abstraction: semantic actions, adapters        | SDL3, engine/         |
| `render/`   | OpenGL rendering: shaders, UI draw, text             | SDL3, SDL3_ttf, glad, engine/ |
| `scene/`    | Game scenes/states: title, save select, settings     | engine/, input/, render/, data/ |
| `data/`     | Persistence: strings, settings, save data            | cJSON, libyaml, platform/ |
| `platform/` | Platform abstraction: save paths, feature flags      | SDL3                  |

## Files

- **`main.c`** — Entry point. Initializes the engine and starts the game loop.

## Architecture Notes

- Game logic is fully separated from rendering and input (per design doc §3.5).
- Input flows: SDL events → adapters → semantic actions → scenes.
- States use a vtable interface (`State` struct with function pointers).
- All rendering uses orthographic projection for UI; 3D diorama rendering will be added later.
