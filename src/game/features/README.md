# src/game/features/

Built-in environmental feature implementations. Each feature is a self-contained module that implements the Feature vtable from `feature.h`.

## Files

| File                            | Purpose |
|--------------------------------|---------|
| `feature_registry.h / feature_registry.c` | Registers all built-in feature factories with the level loader. Call `feature_registry_init()` once at startup before loading levels. |
| `spike_trap.h / spike_trap.c`  | Spike trap feature. Reactive — triggered by Theseus stepping on/off the tile (not a fixed timer). Spikes shoot up the turn after Theseus triggers them, stay up for one turn. Hazardous to Theseus when active; blocks Minotaur when active. Only Theseus triggers. |

## Planned Features

| Feature | Description |
|---------|-------------|
| `pressure_plate` | Permanently toggles linked walls/tile passability when Theseus steps on it. Color-coded for visual association. |
| `locking_gate` | One-way door that locks permanently after any actor passes through. |
| `auto_turnstile` | Automatic turnstile at 4-tile junction. Rotates 90° each environment phase, moving walls, actors, and features. |
| `manual_turnstile` | Player-activated turnstile. Theseus pushes wall to rotate connected walls. Minotaur cannot trigger. |
| `teleporter` | Paired tiles — step on one, appear at other. Both actors trigger. No chaining. |
| `crumbling_floor` | Passable once, collapses into deadly pit after Theseus leaves. Waiting on it = death. |
| `moving_platform` | Floating tile over void. Moves along a path each environment phase. Actors ride it. |
| `medusa_wall` | Wall-mounted face. Theseus dies if he moves toward it while in line of sight. |
| `ice_tile` | Theseus slides until hitting a wall or non-ice tile. Minotaur unaffected. |
| `groove_box` | Heavy box in a fixed groove track. Theseus can push along groove. Blocks both actors. |

## Adding a New Feature

1. Create `feature_name.h` and `feature_name.c` in this directory.
2. Define a `FeatureVTable` with the hooks your feature needs (leave unused hooks as NULL).
3. Implement a factory function: `Feature* feature_name_create(int col, int row, const cJSON* config)`.
4. Add `#include "feature_name.h"` and a `level_loader_register_feature()` call in `feature_registry.c`.

That's it — the level loader, turn resolver, and undo system pick up the new feature automatically.

## Available Vtable Hooks

| Hook                    | When Called | Use For |
|------------------------|------------|---------|
| `blocks_movement`      | Before any entity moves into/from this tile | Active spike traps (vs Minotaur), groove boxes, locked gates |
| `on_pre_move`          | Before Theseus commits a move (Theseus only) | Medusa line-of-sight kill, ice tile slide detection |
| `on_enter`             | After an entity moves onto this tile | Teleporters, pressure plates, spike trap arming |
| `on_leave`             | After an entity moves off this tile | Spike trap arming, crumbling floor marking |
| `on_push`              | Theseus pushes against blocked tile/wall (Theseus only) | Manual turnstile rotation, groove box pushing |
| `on_environment_phase` | Between Theseus and Minotaur phases | Spike activation/retraction, auto-turnstile rotation, moving platform advance, crumbling floor collapse |
| `is_hazardous`         | After each entity step and after environment phase | Active spike traps, collapsed floors, pit tiles |
| `snapshot_*`           | Undo push/pop | Any feature with mutable state |
