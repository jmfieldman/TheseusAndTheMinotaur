#ifndef GAME_FEATURE_SPIKE_TRAP_H
#define GAME_FEATURE_SPIKE_TRAP_H

#include "../feature.h"

struct cJSON;

/*
 * Spike Trap — reactive spike trap triggered by Theseus.
 *
 * Config:
 *   "initial_active":  bool — whether spikes start up (default: false)
 *   "up_turns":        int  — how many turns the spikes stay up (default: 1)
 *
 * Behaviour:
 *   - Spikes start down (safe). Theseus can step onto the tile freely.
 *   - When Theseus steps onto the tile (on_enter), the trap is ARMED.
 *   - The trap fires on the NEXT turn's environment phase — not the
 *     same turn.  This gives Theseus one full turn to step off.
 *   - Once fired, spikes stay up for "up_turns" environment phases,
 *     then retract.  After retracting, the cycle can repeat.
 *   - While active (spikes up):
 *     - is_hazardous returns true (deadly to Theseus)
 *     - blocks_movement returns true for ENTITY_MINOTAUR
 *   - Minotaur stepping on does NOT arm the trap.
 */

Feature* spike_trap_create(int col, int row, const struct cJSON* config);

#endif /* GAME_FEATURE_SPIKE_TRAP_H */
