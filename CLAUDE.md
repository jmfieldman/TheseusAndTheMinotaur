# Claude Code Instructions

## Build

```bash
cmake -B build && cmake --build build
```

Run: `build/theseus`

## Project Structure

- `design/` — Game design documents (see `design/00-index.md`)
- `src/` — C source code organized by subsystem
- `assets/` — Runtime assets (fonts, strings)
- `third_party/` — Vendored dependencies (glad)

## README Maintenance

**Every directory under `src/`, `assets/`, and `third_party/` has a `README.md`** that documents the files in that directory and their responsibilities.

**When you add, remove, or significantly change files in any directory, update that directory's `README.md` to reflect the change.** This includes:

- Adding a new source file → add it to the directory's README table
- Removing a file → remove it from the README
- Changing a file's purpose significantly → update its description
- Adding a new subdirectory → create a README.md in it

## Code Conventions

- **Language:** C11
- **Naming:** `snake_case` for functions and variables, `PascalCase` for type names, `UPPER_CASE` for macros/enums
- **Includes:** Use `"module/file.h"` for project headers, `<lib.h>` for external libraries
- **Memory:** Explicit `init()`/`shutdown()` or `create()`/`destroy()` pairs. No hidden allocation.
- **State interface:** Scenes implement the `State` vtable (`on_enter`, `on_exit`, `handle_action`, `update`, `render`, `destroy`)
- **Input separation:** Game logic only sees `SemanticAction` values, never raw SDL events
- **Rendering separation:** Game logic never calls OpenGL directly; all drawing goes through `render/` APIs
