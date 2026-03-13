#ifndef GAME_FEATURE_SPIKE_TRAP_H
#define GAME_FEATURE_SPIKE_TRAP_H

#include "../feature.h"

struct cJSON;

/*
 * Spike Trap — reactive spike trap triggered by Theseus.
 *
 * Config:
 *   "initial_active":  bool — whether spikes start up (default: false)
 *
 * Behaviour:
 *   - Spikes start down (safe). Theseus can step onto the tile freely.
 *   - When Theseus LEAVES the tile (on_leave), the trap is ARMED.
 *   - When Theseus WAITS on the tile (on_leave is not called, but the
 *     trap arms via on_enter + environment phase logic), the trap
 *     activates during the environment phase, killing Theseus.
 *   - During the environment phase:
 *     - If armed and not active: spikes shoot UP (active = true, armed = false)
 *     - If active: spikes retract DOWN (active = false) after one turn
 *   - When active (up):
 *     - is_hazardous returns true (deadly to Theseus)
 *     - blocks_movement returns true for ENTITY_MINOTAUR (impassable)
 *   - Minotaur standing on a spike tile does NOT trigger it.
 *   - Only Theseus triggers arming.
 */

Feature* spike_trap_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_SPIKE_TRAP_H */
