# src/platform/

Platform abstraction layer. Isolates OS-specific code behind a common interface.

## Files

| File                | Purpose |
|---------------------|---------|
| `platform.h / platform.c` | Platform detection and path resolution. Uses `SDL_GetPrefPath()` for save directory and `SDL_GetBasePath()` for asset directory. Provides `platform_is_mobile()` and `platform_show_quit()` for UI decisions. `platform_ensure_dir()` creates directories. |

## Save Paths by Platform

| Platform    | Save Directory                                         |
|-------------|--------------------------------------------------------|
| macOS       | `~/Library/Application Support/com.theseusandtheminotaur/TheseusAndTheMinotaur/` |
| Windows     | `%APPDATA%/TheseusAndTheMinotaur/TheseusAndTheMinotaur/` |
| Linux       | `~/.local/share/theseusandtheminotaur/TheseusAndTheMinotaur/` |
| iOS/iPadOS  | App sandbox (via `SDL_GetPrefPath`)                     |
| tvOS        | App sandbox (via `SDL_GetPrefPath`)                     |

## Future

- iOS app lifecycle handling (backgrounding → auto-save)
- tvOS focus engine integration
- Steam Cloud save sync
