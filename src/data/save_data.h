#ifndef SAVE_DATA_H
#define SAVE_DATA_H

#include <stdbool.h>
#include <stdint.h>

#define SAVE_SLOT_COUNT    3
#define MAX_BIOME_NAME     64
#define MAX_NODE_NAME      64
#define MAX_LEVELS_PER_BIOME 20
#define MAX_BIOMES         16

typedef struct {
    char     id[64];
    bool     completed;
    int      best_turns;
    int      stars;       /* 0, 1, or 2 */
} SaveLevelEntry;

typedef struct {
    char             name[MAX_BIOME_NAME];
    bool             unlocked;
    SaveLevelEntry   levels[MAX_LEVELS_PER_BIOME];
    int              level_count;
} SaveBiomeEntry;

typedef struct {
    int              slot;            /* 0, 1, or 2 */
    bool             exists;          /* whether this slot has data */
    uint32_t         play_time_secs;
    char             current_biome[MAX_BIOME_NAME];
    char             current_node[MAX_NODE_NAME];
    SaveBiomeEntry   biomes[MAX_BIOMES];
    int              biome_count;
} SaveSlot;

/* Load a save slot from disk. Sets slot->exists = false if not found. */
void save_data_load(SaveSlot* slot, int slot_index);

/* Save a slot to disk. */
void save_data_save(const SaveSlot* slot);

/* Check if a save file exists for the given slot. */
bool save_data_exists(int slot_index);

/* Delete a save file. */
void save_data_delete(int slot_index);

/* Get the file path for a save slot. */
void save_data_get_path(int slot_index, char* out, int out_size);

/* Get summary info for display (returns formatted strings). */
int  save_data_total_stars(const SaveSlot* slot);
int  save_data_total_levels_completed(const SaveSlot* slot);
int  save_data_total_levels(const SaveSlot* slot);
void save_data_format_playtime(uint32_t secs, char* out, int out_size);

#endif /* SAVE_DATA_H */
