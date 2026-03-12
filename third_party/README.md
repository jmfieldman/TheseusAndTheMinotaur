# third_party/

Vendored third-party dependencies that are committed to the repository (not fetched via CMake).

## Files

| Directory     | Purpose |
|---------------|---------|
| `glad/` | OpenGL 3.3 Core function loader. Generated via [glad2](https://github.com/Dav1dde/glad). Contains `include/glad/gl.h`, `include/KHR/khrplatform.h`, and `src/gl.c`. |

## Other Dependencies

These are fetched automatically by CMake via `FetchContent` (not stored here):

| Library    | Version | Purpose |
|-----------|---------|---------|
| SDL3      | main    | Windowing, input events, audio, platform abstraction |
| SDL3_ttf  | main    | TrueType font rendering (vendored freetype + harfbuzz) |
| cJSON     | v1.7.18 | JSON parsing for level data and string tables |
| libyaml   | 0.2.5   | YAML parsing for settings and save files |

## Regenerating glad

```bash
pip3 install glad2
python3 -m glad --api gl:core=3.3 --out-path third_party/glad c
```
