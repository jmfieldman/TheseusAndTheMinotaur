#ifndef STRINGS_H
#define STRINGS_H

#include <stdbool.h>

/* Load the string table from a JSON file. */
bool strings_init(const char* json_path);

/* Shutdown and free the string table. */
void strings_shutdown(void);

/* Look up a string by key. Returns the key itself if not found. */
const char* strings_get(const char* key);

#endif /* STRINGS_H */
