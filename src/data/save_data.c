#include "data/save_data.h"
#include "platform/platform.h"
#include "engine/utils.h"

#include <stdio.h>
#include <string.h>

void save_data_get_path(int slot_index, char* out, int out_size) {
    snprintf(out, out_size, "%ssave_%d.yml", platform_get_save_dir(), slot_index);
}

bool save_data_exists(int slot_index) {
    char path[512];
    save_data_get_path(slot_index, path, sizeof(path));
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return true; }
    return false;
}

void save_data_load(SaveSlot* slot, int slot_index) {
    memset(slot, 0, sizeof(SaveSlot));
    slot->slot = slot_index;
    slot->exists = false;

    char path[512];
    save_data_get_path(slot_index, path, sizeof(path));

    FILE* f = fopen(path, "r");
    if (!f) return;

    /* Simple line-based parser for our known YAML format.
     * For a real game we'd use libyaml properly, but this gets us started. */
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[128], val[128];
        if (sscanf(line, " play_time_seconds: %u", &slot->play_time_secs) == 1) continue;
        if (sscanf(line, " current_biome: %127s", val) == 1) {
            strncpy(slot->current_biome, val, MAX_BIOME_NAME - 1);
            continue;
        }
        if (sscanf(line, " current_node: %127s", val) == 1) {
            strncpy(slot->current_node, val, MAX_NODE_NAME - 1);
            continue;
        }
        (void)key;
    }

    fclose(f);
    slot->exists = true;
    LOG_DEBUG("Save slot %d loaded from %s", slot_index, path);
}

void save_data_save(const SaveSlot* slot) {
    char path[512];
    save_data_get_path(slot->slot, path, sizeof(path));

    FILE* f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("Failed to save slot %d to %s", slot->slot, path);
        return;
    }

    fprintf(f, "slot: %d\n", slot->slot);
    fprintf(f, "version: 1\n");
    fprintf(f, "play_time_seconds: %u\n", slot->play_time_secs);
    fprintf(f, "current_biome: %s\n", slot->current_biome);
    fprintf(f, "current_node: %s\n", slot->current_node);

    /* Write biome data */
    fprintf(f, "\nbiomes:\n");
    for (int b = 0; b < slot->biome_count; b++) {
        const SaveBiomeEntry* biome = &slot->biomes[b];
        fprintf(f, "  %s:\n", biome->name);
        fprintf(f, "    unlocked: %s\n", biome->unlocked ? "true" : "false");
        fprintf(f, "    levels:\n");
        for (int l = 0; l < biome->level_count; l++) {
            const SaveLevelEntry* level = &biome->levels[l];
            fprintf(f, "      - id: %s\n", level->id);
            fprintf(f, "        completed: %s\n", level->completed ? "true" : "false");
            fprintf(f, "        best_turns: %d\n", level->best_turns);
            fprintf(f, "        stars: %d\n", level->stars);
        }
    }

    fclose(f);
    LOG_INFO("Save slot %d saved to %s", slot->slot, path);
}

void save_data_delete(int slot_index) {
    char path[512];
    save_data_get_path(slot_index, path, sizeof(path));
    remove(path);
    LOG_INFO("Save slot %d deleted", slot_index);
}

int save_data_total_stars(const SaveSlot* slot) {
    int total = 0;
    for (int b = 0; b < slot->biome_count; b++) {
        for (int l = 0; l < slot->biomes[b].level_count; l++) {
            total += slot->biomes[b].levels[l].stars;
        }
    }
    return total;
}

int save_data_total_levels_completed(const SaveSlot* slot) {
    int total = 0;
    for (int b = 0; b < slot->biome_count; b++) {
        for (int l = 0; l < slot->biomes[b].level_count; l++) {
            if (slot->biomes[b].levels[l].completed) total++;
        }
    }
    return total;
}

int save_data_total_levels(const SaveSlot* slot) {
    int total = 0;
    for (int b = 0; b < slot->biome_count; b++) {
        total += slot->biomes[b].level_count;
    }
    return total;
}

void save_data_format_playtime(uint32_t secs, char* out, int out_size) {
    int hours   = secs / 3600;
    int minutes = (secs % 3600) / 60;
    int seconds = secs % 60;
    snprintf(out, out_size, "%d:%02d:%02d", hours, minutes, seconds);
}
