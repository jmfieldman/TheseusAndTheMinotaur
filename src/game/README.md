# src/game/

Pure game logic module. No rendering, no SDL, no platform dependencies. Fully testable in isolation.

## Files

| File                      | Purpose |
|--------------------------|---------|
| `game.h`                 | Convenience header — includes the entire game logic API. |
| `feature.h / feature.c`  | Feature vtable interface. Defines pluggable hook points (`blocks_movement`, `on_pre_move`, `on_enter`, `on_leave`, `on_push`, `on_environment_phase`, `is_hazardous`, snapshot support). Each environmental element implements this interface. Includes `PreMoveResult` enum for pre-move checks (OK, KILL, SLIDE). |
| `grid.h / grid.c`        | Grid and Cell model. Manages the NxM tile grid, walls (stored per-edge, mirrored on neighbours), impassable tiles, entity positions, and per-cell feature lists. Provides movement queries and entity move execution with on_enter/on_leave callbacks. |
| `turn.h / turn.c`        | Turn resolution loop. Executes the fixed 3-phase sequence: Theseus phase → Environment phase → Minotaur phase. Checks win/loss conditions after each phase. |
| `minotaur.h / minotaur.c` | Minotaur AI. Greedy 2-step chase with horizontal priority. Cannot exit through exit door. Immune to hazards but blocked by walls/impassable tiles. |
| `undo.h / undo.c`        | Undo/redo snapshot stack. Captures entity positions and all mutable feature state (via vtable snapshot hooks). Supports unlimited undo depth and full reset to initial state. |
| `level_loader.h / level_loader.c` | Level JSON parser. Reads level files (via cJSON), creates Grid with walls/impassable tiles, and instantiates features via a pluggable factory registry. |

## Subdirectories

| Directory    | Purpose |
|-------------|---------|
| `features/` | Built-in feature implementations (spike trap, pressure plate, turnstiles, teleporter, etc.). Each feature is a self-contained .c file that implements the Feature vtable. |

## Architecture Notes

- **Pluggable features:** New environmental mechanics are added by writing a single .c file with the relevant vtable hooks and registering the factory in `feature_registry.c`. No changes to the turn loop, Minotaur AI, or undo system required.
- **Pre-move hooks:** Features like Medusa walls and ice tiles use `on_pre_move` to intervene before Theseus's move commits (kill or slide).
- **Push hooks:** Features like manual turnstiles and groove boxes use `on_push` to react when Theseus pushes against a blocked tile.
- **Coordinate convention:** Columns increase W→E, rows increase S→N. South = bottom/camera-facing.
- **Feature evaluation order:** Features are evaluated in array order (placement order from level data) within each phase.
- **The Minotaur is immortal:** Immune to all hazards, but can be blocked by walls, impassable tiles, and movement-blocking features. The Minotaur does not trigger reactive features (spike traps, pressure plates, manual turnstiles).
- **Exit door:** Implemented as a removed boundary wall. The Minotaur is explicitly blocked from exiting (checked in `minotaur.c`).
