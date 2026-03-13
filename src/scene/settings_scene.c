#include "scene/settings_scene.h"
#include "engine/engine.h"
#include "engine/utils.h"
#include "render/renderer.h"
#include "render/ui_draw.h"
#include "render/text_render.h"
#include "data/strings.h"
#include "data/settings.h"
#include "input/input_manager.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef enum {
    SETTING_MUSIC_VOLUME,
    SETTING_SFX_VOLUME,
    SETTING_ANIM_SPEED,
    SETTING_CAMERA_FOV,
    SETTING_FULLSCREEN,
    SETTING_COUNT
} SettingItem;

typedef struct {
    State    base;
    int      selected;
    float    time;
    int      visible_count;
    SettingItem visible_items[SETTING_COUNT];
} SettingsScene;

static void rebuild_visible(SettingsScene* ss) {
    ss->visible_count = 0;
    ss->visible_items[ss->visible_count++] = SETTING_MUSIC_VOLUME;
    ss->visible_items[ss->visible_count++] = SETTING_SFX_VOLUME;
    ss->visible_items[ss->visible_count++] = SETTING_ANIM_SPEED;
    ss->visible_items[ss->visible_count++] = SETTING_CAMERA_FOV;
    if (!platform_is_mobile()) {
        ss->visible_items[ss->visible_count++] = SETTING_FULLSCREEN;
    }
}

static void settings_on_enter(State* self) {
    SettingsScene* ss = (SettingsScene*)self;
    ss->selected = 0;
    ss->time = 0.0f;
    rebuild_visible(ss);
    input_manager_set_context(INPUT_CONTEXT_MENU);
    LOG_INFO("Settings screen entered");
}

static void settings_on_exit(State* self) {
    (void)self;
    /* Save settings on exit */
    char path[512];
    snprintf(path, sizeof(path), "%s/settings.yml", platform_get_save_dir());
    settings_save(path);
}

static void settings_handle_action(State* self, SemanticAction action) {
    SettingsScene* ss = (SettingsScene*)self;

    switch (action) {
    case ACTION_UI_UP:
        ss->selected--;
        if (ss->selected < 0) ss->selected = ss->visible_count - 1;
        break;
    case ACTION_UI_DOWN:
        ss->selected++;
        if (ss->selected >= ss->visible_count) ss->selected = 0;
        break;
    case ACTION_UI_LEFT:
    case ACTION_UI_RIGHT: {
        SettingItem item = ss->visible_items[ss->selected];
        switch (item) {
        case SETTING_MUSIC_VOLUME: {
            float delta = (action == ACTION_UI_RIGHT) ? 0.05f : -0.05f;
            g_settings.music_volume = CLAMP(g_settings.music_volume + delta, 0.0f, 1.0f);
            break;
        }
        case SETTING_SFX_VOLUME: {
            float delta = (action == ACTION_UI_RIGHT) ? 0.05f : -0.05f;
            g_settings.sfx_volume = CLAMP(g_settings.sfx_volume + delta, 0.0f, 1.0f);
            break;
        }
        case SETTING_ANIM_SPEED: {
            float delta = (action == ACTION_UI_RIGHT) ? 0.25f : -0.25f;
            g_settings.anim_speed = CLAMP(g_settings.anim_speed + delta, 1.0f, 4.0f);
            break;
        }
        case SETTING_CAMERA_FOV: {
            float delta = (action == ACTION_UI_RIGHT) ? 5.0f : -5.0f;
            g_settings.camera_fov = CLAMP(g_settings.camera_fov + delta, 5.0f, 90.0f);
            break;
        }
        case SETTING_FULLSCREEN:
            g_settings.fullscreen = !g_settings.fullscreen;
            /* TODO: actually toggle fullscreen via SDL */
            break;
        default:
            break;
        }
        break;
    }
    case ACTION_UI_BACK:
        engine_pop_state();
        break;
    case ACTION_UI_CONFIRM:
        /* Toggle for fullscreen, no-op for sliders */
        if (ss->visible_items[ss->selected] == SETTING_FULLSCREEN) {
            g_settings.fullscreen = !g_settings.fullscreen;
        }
        break;
    default:
        break;
    }
}

static void settings_update(State* self, float dt) {
    SettingsScene* ss = (SettingsScene*)self;
    ss->time += dt;
}

static void settings_render(State* self) {
    SettingsScene* ss = (SettingsScene*)self;
    int vw, vh;
    renderer_get_viewport(&vw, &vh);

    float cx = vw * 0.5f;

    /* Semi-transparent overlay background */
    ui_draw_rect(0, 0, (float)vw, (float)vh, color_rgba(0, 0, 0, 0.7f));

    /* Panel */
    float panel_w = fminf(500.0f, vw * 0.8f);
    float panel_h = 410.0f;
    float panel_x = cx - panel_w * 0.5f;
    float panel_y = vh * 0.5f - panel_h * 0.5f;

    ui_draw_rect_rounded(panel_x, panel_y, panel_w, panel_h, 12.0f,
                         color_hex(0x1A1A20));

    /* Title */
    text_render_draw(strings_get("settings_title"),
                     cx, panel_y + 20.0f, TEXT_SIZE_TITLE,
                     color_rgba(0.85f, 0.78f, 0.60f, 1.0f),
                     TEXT_ALIGN_CENTER);

    /* Items */
    float item_y_start = panel_y + 90.0f;
    float item_spacing = 55.0f;
    float label_x = panel_x + 30.0f;
    float value_x = panel_x + panel_w - 30.0f;

    for (int i = 0; i < ss->visible_count; i++) {
        float y = item_y_start + i * item_spacing;
        bool selected = (i == ss->selected);
        SettingItem item = ss->visible_items[i];

        Color label_col = selected
            ? color_rgba(0.95f, 0.88f, 0.65f, 1.0f)
            : color_rgba(0.6f, 0.58f, 0.52f, 1.0f);

        /* Selection highlight */
        if (selected) {
            float pulse = 0.8f + 0.2f * sinf(ss->time * 3.0f);
            ui_draw_rect_rounded(panel_x + 10.0f, y - 6.0f,
                                 panel_w - 20.0f, 40.0f, 6.0f,
                                 color_rgba(0.85f, 0.78f, 0.60f, 0.08f * pulse));
        }

        const char* label = "";
        char value_str[32] = "";

        switch (item) {
        case SETTING_MUSIC_VOLUME:
            label = strings_get("settings_music");
            snprintf(value_str, sizeof(value_str), "%d%%",
                     (int)(g_settings.music_volume * 100.0f + 0.5f));
            break;
        case SETTING_SFX_VOLUME:
            label = strings_get("settings_sfx");
            snprintf(value_str, sizeof(value_str), "%d%%",
                     (int)(g_settings.sfx_volume * 100.0f + 0.5f));
            break;
        case SETTING_ANIM_SPEED:
            label = strings_get("settings_anim_speed");
            snprintf(value_str, sizeof(value_str), "%.1fx", g_settings.anim_speed);
            break;
        case SETTING_CAMERA_FOV:
            label = strings_get("settings_camera_fov");
            snprintf(value_str, sizeof(value_str), "%.0f\xC2\xB0", g_settings.camera_fov);
            break;
        case SETTING_FULLSCREEN:
            label = strings_get("settings_fullscreen");
            snprintf(value_str, sizeof(value_str), "%s",
                     g_settings.fullscreen
                         ? strings_get("settings_on")
                         : strings_get("settings_off"));
            break;
        default:
            break;
        }

        text_render_draw(label, label_x, y, TEXT_SIZE_BODY, label_col,
                         TEXT_ALIGN_LEFT);
        text_render_draw(value_str, value_x, y, TEXT_SIZE_BODY, label_col,
                         TEXT_ALIGN_RIGHT);

        /* Slider bar for adjustable items */
        if (item == SETTING_MUSIC_VOLUME || item == SETTING_SFX_VOLUME ||
            item == SETTING_ANIM_SPEED || item == SETTING_CAMERA_FOV) {
            float bar_x = panel_x + panel_w * 0.45f;
            float bar_w = panel_w * 0.35f;
            float bar_y = y + 10.0f;
            float bar_h = 6.0f;
            float fill;
            if (item == SETTING_ANIM_SPEED)
                fill = (g_settings.anim_speed - 1.0f) / 3.0f;
            else if (item == SETTING_CAMERA_FOV)
                fill = (g_settings.camera_fov - 5.0f) / 85.0f;
            else if (item == SETTING_MUSIC_VOLUME)
                fill = g_settings.music_volume;
            else
                fill = g_settings.sfx_volume;

            /* Track */
            ui_draw_rect_rounded(bar_x, bar_y, bar_w, bar_h, 3.0f,
                                 color_rgba(0.25f, 0.25f, 0.28f, 1.0f));
            /* Fill */
            if (fill > 0.01f) {
                ui_draw_rect_rounded(bar_x, bar_y, bar_w * fill, bar_h, 3.0f,
                                     selected
                                         ? color_rgba(0.85f, 0.78f, 0.60f, 1.0f)
                                         : color_rgba(0.5f, 0.48f, 0.42f, 1.0f));
            }
        }
    }

    /* Back hint */
    text_render_draw("[Esc] Back", cx, panel_y + panel_h - 30.0f,
                     TEXT_SIZE_SMALL,
                     color_rgba(0.4f, 0.38f, 0.35f, 0.8f),
                     TEXT_ALIGN_CENTER);
}

static void settings_destroy(State* self) {
    free(self);
}

State* settings_scene_create(void) {
    SettingsScene* ss = (SettingsScene*)calloc(1, sizeof(SettingsScene));

    ss->base.on_enter      = settings_on_enter;
    ss->base.on_exit       = settings_on_exit;
    ss->base.on_pause      = NULL;
    ss->base.on_resume     = settings_on_enter;
    ss->base.handle_action = settings_handle_action;
    ss->base.update        = settings_update;
    ss->base.render        = settings_render;
    ss->base.destroy       = settings_destroy;
    ss->base.transparent   = true;  /* render states below (for overlay effect) */

    return (State*)ss;
}
