#include "feature_registry.h"
#include "../level_loader.h"
#include "spike_trap.h"

/*
 * Register all built-in feature types.
 *
 * To add a new feature:
 *   1. Create feature_name.h / feature_name.c in src/game/features/
 *   2. Implement the factory: Feature* feature_name_create(int, int, const cJSON*)
 *   3. #include the header here
 *   4. Add a level_loader_register_feature() call below
 *
 * That's it — the level loader, turn resolver, and undo system
 * pick up the new feature automatically via the vtable.
 */
void feature_registry_init(void) {
    level_loader_register_feature("spike_trap", spike_trap_create);

    /* Future features register here:
     * level_loader_register_feature("pressure_plate",   pressure_plate_create);
     * level_loader_register_feature("locking_gate",     locking_gate_create);
     * level_loader_register_feature("auto_turnstile",   auto_turnstile_create);
     * level_loader_register_feature("manual_turnstile", manual_turnstile_create);
     * level_loader_register_feature("teleporter",       teleporter_create);
     * level_loader_register_feature("crumbling_floor",  crumbling_floor_create);
     * level_loader_register_feature("moving_platform",  moving_platform_create);
     * level_loader_register_feature("medusa_wall",      medusa_wall_create);
     * level_loader_register_feature("ice_tile",         ice_tile_create);
     * level_loader_register_feature("groove_box",       groove_box_create);
     */
}
