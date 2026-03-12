# src/engine/

Core engine systems: initialization, game loop, state management, and shared utilities.

## Files

| File                | Purpose |
|---------------------|---------|
| `engine.h / engine.c` | Global `Engine` singleton. Owns the SDL window, GL context, and game loop. Provides `engine_init()`, `engine_run()`, `engine_shutdown()`, and state management shortcuts (`engine_push_state()`, etc.). Uses a fixed-timestep accumulator (60Hz). |
| `state_manager.h / state_manager.c` | Stack-based state machine. `State` is a vtable struct with lifecycle callbacks (`on_enter`, `on_exit`, `on_pause`, `on_resume`), per-frame hooks (`handle_action`, `update`, `render`), and `destroy`. Supports overlay rendering (bottom-up) for transparent states. Max stack depth: 8. |
| `utils.h` | Shared macros and types. `LOG_INFO/WARN/ERROR/DEBUG` logging, `Color` struct with constructors (`color_hex`, `color_rgba`), math helpers (`CLAMP`, `LERP`, `MIN`, `MAX`), and `ARRAY_LEN`. |
