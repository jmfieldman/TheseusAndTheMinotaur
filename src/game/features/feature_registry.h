#ifndef GAME_FEATURE_REGISTRY_H
#define GAME_FEATURE_REGISTRY_H

/*
 * Feature registry — registers all built-in feature factories
 * with the level loader.
 *
 * Call once at startup (before loading any levels).
 * New features just need to add their registration call here.
 */
void feature_registry_init(void);

#endif /* GAME_FEATURE_REGISTRY_H */
