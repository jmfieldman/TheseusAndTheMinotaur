#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

/* Initialize platform layer. */
void platform_init(void);

/* Get the directory for save files and settings. */
const char* platform_get_save_dir(void);

/* Get the directory for game assets (fonts, strings, etc.) */
const char* platform_get_asset_dir(void);

/* Ensure a directory exists, creating it if necessary. */
bool platform_ensure_dir(const char* path);

/* Is this a mobile/touch platform? */
bool platform_is_mobile(void);

/* Should the Quit menu item be shown? (No on iOS/tvOS) */
bool platform_show_quit(void);

#endif /* PLATFORM_H */
