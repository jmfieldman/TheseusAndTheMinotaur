#ifndef TEST_LEVEL_SCENE_H
#define TEST_LEVEL_SCENE_H

#include "engine/state_manager.h"

/*
 * Test level selector scene.
 *
 * Scans all level JSON files from assets/levels/ and presents them
 * as a scrollable menu. Selecting a level launches the puzzle scene.
 * Back returns to the title screen.
 */
State* test_level_scene_create(void);

#endif /* TEST_LEVEL_SCENE_H */
