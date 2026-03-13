#include "scene/title_scene.h"
#include "scene/save_select_scene.h"
#include "scene/settings_scene.h"
#include "scene/test_level_scene.h"
#include "engine/engine.h"
#include "engine/utils.h"
#include "render/renderer.h"
#include "render/ui_draw.h"
#include "render/text_render.h"
#include "data/strings.h"
#include "data/save_data.h"
#include "input/input_manager.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <math.h>

/* ---------- Menu items ---------- */
typedef enum {
    TITLE_ITEM_PLAY,
    TITLE_ITEM_CONTINUE,
    TITLE_ITEM_SETTINGS,
    TITLE_ITEM_TEST,
    TITLE_ITEM_QUIT,
    TITLE_ITEM_COUNT
} TitleMenuItem;

typedef struct {
    State       base;
    int         selected;
    bool        has_continue;   /* show Continue if a save exists */
    bool        show_quit;      /* platform-dependent */
    float       time;           /* for animations */
    float       fade_in;        /* 0 to 1 fade-in progress */

    /* Visible menu items (skipping hidden ones) */
    TitleMenuItem visible_items[TITLE_ITEM_COUNT];
    int           visible_count;
} TitleScene;

/* ---------- Helpers ---------- */

static void rebuild_visible_items(TitleScene* ts) {
    ts->visible_count = 0;
    ts->visible_items[ts->visible_count++] = TITLE_ITEM_PLAY;
    if (ts->has_continue)
        ts->visible_items[ts->visible_count++] = TITLE_ITEM_CONTINUE;
    ts->visible_items[ts->visible_count++] = TITLE_ITEM_SETTINGS;
    ts->visible_items[ts->visible_count++] = TITLE_ITEM_TEST;
    if (ts->show_quit)
        ts->visible_items[ts->visible_count++] = TITLE_ITEM_QUIT;
}

static const char* item_string_key(TitleMenuItem item) {
    switch (item) {
    case TITLE_ITEM_PLAY:     return "title_play";
    case TITLE_ITEM_CONTINUE: return "title_continue";
    case TITLE_ITEM_SETTINGS: return "title_settings";
    case TITLE_ITEM_TEST:     return "title_test";
    case TITLE_ITEM_QUIT:     return "title_quit";
    default:                  return "";
    }
}

/* ---------- State callbacks ---------- */

static void title_on_enter(State* self) {
    TitleScene* ts = (TitleScene*)self;
    ts->selected = 0;
    ts->time = 0.0f;
    ts->fade_in = 0.0f;

    /* Check for existing saves */
    ts->has_continue = false;
    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (save_data_exists(i)) {
            ts->has_continue = true;
            break;
        }
    }
    ts->show_quit = platform_show_quit();
    rebuild_visible_items(ts);

    input_manager_set_context(INPUT_CONTEXT_MENU);
    LOG_INFO("Title screen entered");
}

static void title_on_exit(State* self) {
    (void)self;
}

static void title_on_resume(State* self) {
    TitleScene* ts = (TitleScene*)self;
    /* Re-check saves (may have been deleted in save select) */
    ts->has_continue = false;
    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (save_data_exists(i)) {
            ts->has_continue = true;
            break;
        }
    }
    rebuild_visible_items(ts);
    if (ts->selected >= ts->visible_count) {
        ts->selected = ts->visible_count - 1;
    }
    input_manager_set_context(INPUT_CONTEXT_MENU);
}

static void title_handle_action(State* self, SemanticAction action) {
    TitleScene* ts = (TitleScene*)self;

    switch (action) {
    case ACTION_UI_UP:
        ts->selected--;
        if (ts->selected < 0) ts->selected = ts->visible_count - 1;
        break;

    case ACTION_UI_DOWN:
        ts->selected++;
        if (ts->selected >= ts->visible_count) ts->selected = 0;
        break;

    case ACTION_UI_CONFIRM: {
        TitleMenuItem chosen = ts->visible_items[ts->selected];
        switch (chosen) {
        case TITLE_ITEM_PLAY:
            engine_push_state(save_select_scene_create());
            break;
        case TITLE_ITEM_CONTINUE:
            /* TODO: load last save and go to overworld */
            LOG_INFO("Continue: not yet implemented");
            break;
        case TITLE_ITEM_SETTINGS:
            engine_push_state(settings_scene_create());
            break;
        case TITLE_ITEM_TEST:
            engine_push_state(test_level_scene_create());
            break;
        case TITLE_ITEM_QUIT:
            engine_quit();
            break;
        default:
            break;
        }
        break;
    }

    default:
        break;
    }
}

static void title_update(State* self, float dt) {
    TitleScene* ts = (TitleScene*)self;
    ts->time += dt;
    if (ts->fade_in < 1.0f) {
        ts->fade_in += dt * 2.0f; /* 0.5s fade in */
        if (ts->fade_in > 1.0f) ts->fade_in = 1.0f;
    }
}

static void title_render(State* self) {
    TitleScene* ts = (TitleScene*)self;
    int vw, vh;
    renderer_get_viewport(&vw, &vh);

    float alpha = ts->fade_in;
    float cx = vw * 0.5f;

    /* Background */
    renderer_clear(color_hex(0x121218));

    /* Title text */
    float title_y = vh * 0.20f;
    text_render_draw(strings_get("game_title_line1"),
                     cx, title_y, TEXT_SIZE_TITLE,
                     color_rgba(0.85f, 0.78f, 0.60f, alpha),
                     TEXT_ALIGN_CENTER);
    text_render_draw(strings_get("game_title_line2"),
                     cx, title_y + 55.0f, TEXT_SIZE_HERO,
                     color_rgba(0.85f, 0.78f, 0.60f, alpha),
                     TEXT_ALIGN_CENTER);

    /* Menu items */
    float menu_y = vh * 0.50f;
    float item_spacing = 50.0f;

    for (int i = 0; i < ts->visible_count; i++) {
        TitleMenuItem item = ts->visible_items[i];
        const char* label = strings_get(item_string_key(item));
        float y = menu_y + i * item_spacing;

        bool selected = (i == ts->selected);

        /* Selection indicator */
        if (selected) {
            float pulse = 0.8f + 0.2f * sinf(ts->time * 4.0f);
            int tw, th;
            text_render_measure(label, TEXT_SIZE_LARGE, &tw, &th);

            /* Highlight background */
            float pad = 12.0f;
            ui_draw_rect_rounded(cx - tw * 0.5f - pad, y - 4.0f,
                                 tw + pad * 2, th + 8.0f,
                                 6.0f,
                                 color_rgba(0.85f, 0.78f, 0.60f, 0.15f * pulse * alpha));

            /* Selected text (brighter) */
            text_render_draw(label, cx, y, TEXT_SIZE_LARGE,
                             color_rgba(0.95f, 0.88f, 0.65f, alpha),
                             TEXT_ALIGN_CENTER);

            /* Arrow indicator */
            text_render_draw("\xE2\x96\xB6", cx - tw * 0.5f - 30.0f, y, TEXT_SIZE_LARGE,
                             color_rgba(0.95f, 0.88f, 0.65f, alpha * pulse),
                             TEXT_ALIGN_CENTER);
        } else {
            text_render_draw(label, cx, y, TEXT_SIZE_LARGE,
                             color_rgba(0.6f, 0.58f, 0.52f, alpha * 0.8f),
                             TEXT_ALIGN_CENTER);
        }
    }
}

static void title_destroy(State* self) {
    free(self);
}

/* ---------- Create ---------- */

State* title_scene_create(void) {
    TitleScene* ts = (TitleScene*)calloc(1, sizeof(TitleScene));

    ts->base.on_enter      = title_on_enter;
    ts->base.on_exit       = title_on_exit;
    ts->base.on_pause      = NULL;
    ts->base.on_resume     = title_on_resume;
    ts->base.handle_action = title_handle_action;
    ts->base.update        = title_update;
    ts->base.render        = title_render;
    ts->base.destroy       = title_destroy;
    ts->base.transparent   = false;

    return (State*)ts;
}
