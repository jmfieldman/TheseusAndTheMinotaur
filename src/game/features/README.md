# src/game/features/

Built-in environmental feature implementations. Each feature is a self-contained module that implements the Feature vtable from `feature.h`.

## Files

| File | Purpose |
|------|---------|
| `feature_registry.h / feature_registry.c` | Registers all built-in feature factories with the level loader. Call `feature_registry_init()` once at startup before loading levels. |
| `spike_trap.h / spike_trap.c` | Reactive spike trap triggered by Theseus stepping on/off the tile. Spikes shoot up the turn after Theseus triggers them, stay up for one turn. Hazardous to Theseus when active; blocks Minotaur when active. |
| `pressure_plate.h / pressure_plate.c` | Permanently toggles linked walls/tile passability when Theseus steps on it. Color-coded tints for visual association. Minotaur does not trigger. |
| `locking_gate.h / locking_gate.c` | One-way door that locks permanently (wall goes up) after any actor passes through. |
| `auto_turnstile.h / auto_turnstile.c` | Automatic turnstile at junction of 4 tiles. Rotates 90° each environment phase, moving walls, actors, and features on those tiles. |
| `manual_turnstile.h / manual_turnstile.c` | Player-activated turnstile. Theseus pushes against a wall at the junction to rotate walls 90°. Consumes Theseus's action but he doesn't move. Minotaur cannot trigger. |
| `teleporter.h / teleporter.c` | Paired tiles — step on one, instantly appear at the other. Both actors trigger. No chaining (arrival doesn't re-trigger). |
| `crumbling_floor.h / crumbling_floor.c` | Passable once. Collapses into a deadly pit during environment phase after an actor leaves. Waiting on it causes immediate collapse. Minotaur immune to collapse damage. |
| `moving_platform.h / moving_platform.c` | Floating tile over pit. Moves one step along a defined path each environment phase (pingpong or loop). Actors ride with it. Pit tiles without platform are deadly/impassable. |
| `medusa_wall.h / medusa_wall.c` | Wall-mounted face with line-of-sight. Kills Theseus if he moves toward the Medusa while in its sightline. Walls block sightline. Minotaur immune. |
| `ice_tile.h / ice_tile.c` | Slippery tile. Theseus slides in move direction until hitting a wall or non-ice tile (all within one Theseus phase). Minotaur moves normally on ice. |
| `groove_box.h / groove_box.c` | Heavy box in a fixed groove track. Theseus can push it along the groove. Blocks both actors. Minotaur cannot push. |

## Adding a New Feature

1. Create `feature_name.h` and `feature_name.c` in this directory.
2. Define a `FeatureVTable` with the hooks your feature needs (leave unused hooks as NULL).
3. Implement a factory function: `Feature* feature_name_create(int col, int row, const cJSON* config)`.
4. Add `#include "feature_name.h"` and a `level_loader_register_feature()` call in `feature_registry.c`.

That's it — the level loader, turn resolver, and undo system pick up the new feature automatically.

## Available Vtable Hooks

| Hook | When Called | Use For |
|------|-----------|---------|
| `blocks_movement` | Before any entity moves into/from this tile | Active spike traps (vs Minotaur), groove boxes, locked gates, collapsed pits |
| `on_pre_move` | Before Theseus commits a move (Theseus only) | Medusa line-of-sight kill, ice tile slide detection |
| `on_enter` | After an entity moves onto this tile | Teleporters, pressure plates, spike trap tracking |
| `on_leave` | After an entity moves off this tile | Spike trap arming, crumbling floor marking, locking gate |
| `on_push` | Theseus pushes against blocked tile/wall (Theseus only) | Manual turnstile rotation, groove box pushing |
| `on_environment_phase` | Between Theseus and Minotaur phases | Spike activation/retraction, auto-turnstile rotation, moving platform advance, crumbling floor collapse |
| `is_hazardous` | After each entity step and after environment phase | Active spike traps, collapsed floors, pit tiles without platform |
| `snapshot_*` | Undo push/pop | Any feature with mutable state |
