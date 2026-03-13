# src/engine/

Core engine systems: initialization, game loop, state management, and shared utilities.

## Files

| File                | Purpose |
|---------------------|---------|
| `engine.h / engine.c` | Global `Engine` singleton. Owns the SDL window, GL context, and game loop. Provides `engine_init()`, `engine_run()`, `engine_shutdown()`, and state management shortcuts (`engine_push_state()`, etc.). Uses a fixed-timestep accumulator (60Hz). |
| `state_manager.h / state_manager.c` | Stack-based state machine. `State` is a vtable struct with lifecycle callbacks (`on_enter`, `on_exit`, `on_pause`, `on_resume`), per-frame hooks (`handle_action`, `update`, `render`), and `destroy`. Supports overlay rendering (bottom-up) for transparent states. Max stack depth: 8. |
| `utils.h` | Shared macros and types. `LOG_INFO/WARN/ERROR/DEBUG` logging, `Color` struct with constructors (`color_hex`, `color_rgba`), math helpers (`CLAMP`, `LERP`, `MIN`, `MAX`), and `ARRAY_LEN`. |
| `tween.h / tween.c` | Tween primitives for smooth value interpolation. Supports configurable easing functions (linear, quad, cubic, parabolic arc, out-back). `tween_init()`, `tween_update(dt)`, `tween_value()`. Used by the animation queue for actor movement. |
| `anim_queue.h / anim_queue.c` | Turn animation sequencer. Plays back the visual sequence after game logic resolves instantly: Theseus move → on-leave effects → environment phase → Minotaur step 1 → Minotaur step 2. Supports per-feature animation types: normal hop, ice slide (hop + constant-velocity slide through waypoints), teleport (fade out/in), push (box slides + Theseus steps concurrently), turnstile rotation, spike change, platform move, conveyor push. Environment events play sequentially with type-specific durations. Supports **reverse playback** for undo: plays phases in reverse order at 2× speed with swapped from/to positions. Provides query functions for renderers: teleport progress, aux position (box/platform), rotation progress, current event, ice-slide state, reversing state. |
| `input_buffer.h / input_buffer.c` | Single-slot input buffer for action queuing during animations. Buffer window opens during the Minotaur's last step animation. Only fresh key-down events accepted; last press wins. On animation complete, checks for held keys if no buffer exists. |
