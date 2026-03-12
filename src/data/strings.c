#include "data/strings.h"
#include "engine/utils.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple hash map for string lookup */
#define STRING_MAP_SIZE 128

typedef struct StringEntry {
    char* key;
    char* value;
    struct StringEntry* next;
} StringEntry;

static StringEntry* s_map[STRING_MAP_SIZE] = {0};

static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash % STRING_MAP_SIZE;
}

static void map_put(const char* key, const char* value) {
    unsigned int idx = hash_string(key);
    StringEntry* entry = (StringEntry*)malloc(sizeof(StringEntry));
    entry->key   = strdup(key);
    entry->value = strdup(value);
    entry->next  = s_map[idx];
    s_map[idx]   = entry;
}

static const char* map_get(const char* key) {
    unsigned int idx = hash_string(key);
    StringEntry* entry = s_map[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0) return entry->value;
        entry = entry->next;
    }
    return NULL;
}

bool strings_init(const char* json_path) {
    /* Read file */
    FILE* f = fopen(json_path, "rb");
    if (!f) {
        LOG_WARN("Could not open strings file: %s (using keys as fallback)", json_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    /* Parse JSON */
    cJSON* root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        LOG_ERROR("Failed to parse strings JSON: %s", cJSON_GetErrorPtr());
        return false;
    }

    /* Iterate all string properties */
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (cJSON_IsString(item) && item->string) {
            map_put(item->string, item->valuestring);
        }
    }

    cJSON_Delete(root);
    LOG_INFO("Strings loaded from %s", json_path);
    return true;
}

void strings_shutdown(void) {
    for (int i = 0; i < STRING_MAP_SIZE; i++) {
        StringEntry* entry = s_map[i];
        while (entry) {
            StringEntry* next = entry->next;
            free(entry->key);
            free(entry->value);
            free(entry);
            entry = next;
        }
        s_map[i] = NULL;
    }
}

const char* strings_get(const char* key) {
    const char* val = map_get(key);
    return val ? val : key; /* Return key as fallback */
}
