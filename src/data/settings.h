#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

typedef struct {
    float music_volume;    /* 0.0 - 1.0 */
    float sfx_volume;      /* 0.0 - 1.0 */
    float anim_speed;      /* 1.0 - 4.0: fast-forward multiplier when input is buffered */
    bool  fullscreen;
    int   resolution_w;
    int   resolution_h;
} Settings;

/* Global settings instance. */
extern Settings g_settings;

/* Load settings from YAML file. Uses defaults if file doesn't exist. */
void settings_load(const char* path);

/* Save settings to YAML file. */
void settings_save(const char* path);

/* Reset to default values. */
void settings_default(void);

#endif /* SETTINGS_H */
