# src/input/

Input abstraction layer. Translates raw platform events into semantic actions so game logic never sees SDL scancodes or button IDs.

## Files

| File                | Purpose |
|---------------------|---------|
| `input_types.h` | Defines `SemanticAction` enum (22 actions across UI, Puzzle, and Overworld contexts) and `InputContext` enum. This is the only input header that game logic needs to include. |
| `input_manager.h / input_manager.c` | Central input dispatcher. Polls SDL events, routes them through active adapters, and queues semantic actions in a ring buffer (16 entries). Tracks current `InputContext` which determines how raw input maps to actions. |
| `keyboard_adapter.h / keyboard_adapter.c` | Maps SDL scancodes to semantic actions based on context. Default bindings: WASD + arrows for movement, Enter/Space for confirm, Escape for back/pause, Z for undo, R for reset. |
| `gamepad_adapter.h / gamepad_adapter.c` | Maps SDL gamepad buttons to semantic actions. Supports hot-plug via `SDL_EVENT_GAMEPAD_ADDED/REMOVED`. D-pad for movement, South (A/Cross) for confirm/wait, East (B/Circle) for back/undo. Up to 4 simultaneous gamepads. |

## Design

- Input is **non-blocking**: actions are accepted even during animations (see design doc 01 §10).
- Multiple adapters can be active simultaneously (keyboard + gamepad on desktop).
- The input context is set by scenes when they gain focus (`input_manager_set_context()`).
- Touch adapter (iOS) and Apple Remote adapter (tvOS) will be added as future adapters.
