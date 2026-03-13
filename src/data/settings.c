#include "data/settings.h"
#include "engine/utils.h"

#include <stdio.h>
#include <string.h>
#include <yaml.h>

Settings g_settings;

void settings_default(void) {
    g_settings.music_volume  = 0.8f;
    g_settings.sfx_volume    = 1.0f;
    g_settings.anim_speed    = 2.0f;
    g_settings.fullscreen    = false;
    g_settings.resolution_w  = 1280;
    g_settings.resolution_h  = 720;
    g_settings.camera_perspective = false;
    g_settings.camera_fov    = 20.0f;
}

void settings_load(const char* path) {
    settings_default();

    FILE* f = fopen(path, "rb");
    if (!f) {
        LOG_INFO("No settings file found at %s, using defaults", path);
        return;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        return;
    }

    yaml_parser_set_input_file(&parser, f);

    char current_key[64] = {0};
    bool in_mapping = false;

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;

        if (event.type == YAML_MAPPING_START_EVENT) {
            in_mapping = true;
        } else if (event.type == YAML_MAPPING_END_EVENT) {
            in_mapping = false;
        } else if (event.type == YAML_SCALAR_EVENT && in_mapping) {
            const char* val = (const char*)event.data.scalar.value;
            if (current_key[0] == '\0') {
                strncpy(current_key, val, sizeof(current_key) - 1);
            } else {
                if (strcmp(current_key, "music_volume") == 0)
                    g_settings.music_volume = (float)atof(val);
                else if (strcmp(current_key, "sfx_volume") == 0)
                    g_settings.sfx_volume = (float)atof(val);
                else if (strcmp(current_key, "anim_speed") == 0)
                    g_settings.anim_speed = CLAMP((float)atof(val), 1.0f, 4.0f);
                else if (strcmp(current_key, "fullscreen") == 0)
                    g_settings.fullscreen = (strcmp(val, "true") == 0);
                else if (strcmp(current_key, "resolution_w") == 0)
                    g_settings.resolution_w = atoi(val);
                else if (strcmp(current_key, "resolution_h") == 0)
                    g_settings.resolution_h = atoi(val);
                else if (strcmp(current_key, "camera_perspective") == 0)
                    g_settings.camera_perspective = (strcmp(val, "true") == 0);
                else if (strcmp(current_key, "camera_fov") == 0)
                    g_settings.camera_fov = CLAMP((float)atof(val), 5.0f, 90.0f);

                current_key[0] = '\0';
            }
        } else if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(f);

    LOG_INFO("Settings loaded from %s", path);
}

void settings_save(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("Failed to save settings to %s", path);
        return;
    }

    fprintf(f, "version: 1\n");
    fprintf(f, "music_volume: %.2f\n", g_settings.music_volume);
    fprintf(f, "sfx_volume: %.2f\n",   g_settings.sfx_volume);
    fprintf(f, "anim_speed: %.2f\n",  g_settings.anim_speed);
    fprintf(f, "fullscreen: %s\n",     g_settings.fullscreen ? "true" : "false");
    fprintf(f, "resolution_w: %d\n",   g_settings.resolution_w);
    fprintf(f, "resolution_h: %d\n",   g_settings.resolution_h);
    fprintf(f, "camera_perspective: %s\n", g_settings.camera_perspective ? "true" : "false");
    fprintf(f, "camera_fov: %.1f\n",   g_settings.camera_fov);

    fclose(f);
    LOG_INFO("Settings saved to %s", path);
}
