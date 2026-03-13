#include "scene/save_select_scene.h"
#include "scene/puzzle_scene.h"
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
#include <stdio.h>
#include <math.h>

typedef enum {
    SS_MODE_SELECT,     /* browsing slots */
    SS_MODE_CONFIRM_DELETE  /* delete confirmation dialog */
} SaveSelectMode;

typedef struct {
    State          base;
    int            selected;       /* 0..SAVE_SLOT_COUNT-1 */
    SaveSlot       slots[SAVE_SLOT_COUNT];
    SaveSelectMode mode;
    int            delete_choice;  /* 0 = yes, 1 = no */
    float          time;
} SaveSelectScene;

/* ---------- Helpers ---------- */

static void reload_slots(SaveSelectScene* ss) {
    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        save_data_load(&ss->slots[i], i);
    }
}

/* ---------- State callbacks ---------- */

static void ss_on_enter(State* self) {
    SaveSelectScene* ss = (SaveSelectScene*)self;
    ss->selected = 0;
    ss->mode = SS_MODE_SELECT;
    ss->delete_choice = 1; /* default to "No" for safety */
    ss->time = 0.0f;
    reload_slots(ss);
    input_manager_set_context(INPUT_CONTEXT_MENU);
    LOG_INFO("Save select screen entered");
}

static void ss_on_exit(State* self) {
    (void)self;
}

static void ss_handle_action(State* self, SemanticAction action) {
    SaveSelectScene* ss = (SaveSelectScene*)self;

    if (ss->mode == SS_MODE_CONFIRM_DELETE) {
        switch (action) {
        case ACTION_UI_LEFT:
        case ACTION_UI_RIGHT:
            ss->delete_choice = 1 - ss->delete_choice;
            break;
        case ACTION_UI_CONFIRM:
            if (ss->delete_choice == 0) {
                /* Yes, delete */
                save_data_delete(ss->selected);
                reload_slots(ss);
            }
            ss->mode = SS_MODE_SELECT;
            break;
        case ACTION_UI_BACK:
            ss->mode = SS_MODE_SELECT;
            break;
        default:
            break;
        }
        return;
    }

    /* SS_MODE_SELECT */
    switch (action) {
    case ACTION_UI_UP:
        ss->selected--;
        if (ss->selected < 0) ss->selected = SAVE_SLOT_COUNT - 1;
        break;
    case ACTION_UI_DOWN:
        ss->selected++;
        if (ss->selected >= SAVE_SLOT_COUNT) ss->selected = 0;
        break;
    case ACTION_UI_CONFIRM: {
        if (ss->slots[ss->selected].exists) {
            /* Load existing game — for now, launch tutorial level 1 */
            LOG_INFO("Loading save slot %d (biome: %s)",
                     ss->selected, ss->slots[ss->selected].current_biome);
        } else {
            /* Start new game */
            LOG_INFO("Starting new game in slot %d", ss->selected);
        }
        /* For now, always launch tutorial-01 as a playable prototype */
        char level_path[512];
        snprintf(level_path, sizeof(level_path), "%s/assets/levels/tutorial/tutorial-01.json",
                 platform_get_asset_dir());
        engine_push_state(puzzle_scene_create(level_path));
        break;
    }
    case ACTION_UI_BACK:
        engine_pop_state();
        break;
    case ACTION_UI_RIGHT:
        /* Delete option for existing saves */
        if (ss->slots[ss->selected].exists) {
            ss->mode = SS_MODE_CONFIRM_DELETE;
            ss->delete_choice = 1; /* default to No */
        }
        break;
    default:
        break;
    }
}

static void ss_update(State* self, float dt) {
    SaveSelectScene* ss = (SaveSelectScene*)self;
    ss->time += dt;
}

static void ss_render(State* self) {
    SaveSelectScene* ss = (SaveSelectScene*)self;
    int vw, vh;
    renderer_get_viewport(&vw, &vh);

    float cx = vw * 0.5f;

    /* Background */
    renderer_clear(color_hex(0x121218));

    /* Title */
    text_render_draw(strings_get("save_select_title"),
                     cx, 40.0f, TEXT_SIZE_TITLE,
                     color_rgba(0.85f, 0.78f, 0.60f, 1.0f),
                     TEXT_ALIGN_CENTER);

    /* Save slot cards */
    float card_w = fminf(500.0f, vw * 0.7f);
    float card_h = 100.0f;
    float card_x = cx - card_w * 0.5f;
    float start_y = 130.0f;
    float card_spacing = 120.0f;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        float y = start_y + i * card_spacing;
        bool selected = (i == ss->selected);
        SaveSlot* slot = &ss->slots[i];

        /* Card background */
        Color bg = selected
            ? color_rgba(0.25f, 0.24f, 0.22f, 1.0f)
            : color_rgba(0.16f, 0.16f, 0.18f, 1.0f);
        ui_draw_rect_rounded(card_x, y, card_w, card_h, 8.0f, bg);

        /* Selection border */
        if (selected) {
            float pulse = 0.6f + 0.4f * sinf(ss->time * 3.0f);
            Color border = color_rgba(0.85f, 0.78f, 0.60f, 0.5f * pulse);
            /* Top/bottom borders */
            ui_draw_rect(card_x, y, card_w, 2.0f, border);
            ui_draw_rect(card_x, y + card_h - 2.0f, card_w, 2.0f, border);
            /* Left/right borders */
            ui_draw_rect(card_x, y, 2.0f, card_h, border);
            ui_draw_rect(card_x + card_w - 2.0f, y, 2.0f, card_h, border);
        }

        /* Slot label */
        char label[32];
        snprintf(label, sizeof(label), "Slot %d", i + 1);
        text_render_draw(label, card_x + 20.0f, y + 14.0f,
                         TEXT_SIZE_BODY,
                         color_rgba(0.7f, 0.68f, 0.62f, 1.0f),
                         TEXT_ALIGN_LEFT);

        if (slot->exists) {
            /* Biome name */
            text_render_draw(slot->current_biome, card_x + 20.0f, y + 44.0f,
                             TEXT_SIZE_BODY,
                             color_rgba(0.9f, 0.85f, 0.70f, 1.0f),
                             TEXT_ALIGN_LEFT);

            /* Play time */
            char time_str[32];
            save_data_format_playtime(slot->play_time_secs, time_str, sizeof(time_str));
            text_render_draw(time_str, card_x + card_w - 20.0f, y + 44.0f,
                             TEXT_SIZE_BODY,
                             color_rgba(0.6f, 0.58f, 0.52f, 1.0f),
                             TEXT_ALIGN_RIGHT);

            /* Delete hint when selected */
            if (selected) {
                text_render_draw("[Del \xE2\x86\x92]", card_x + card_w - 20.0f, y + 14.0f,
                                 TEXT_SIZE_SMALL,
                                 color_rgba(0.5f, 0.45f, 0.40f, 0.8f),
                                 TEXT_ALIGN_RIGHT);
            }
        } else {
            text_render_draw(strings_get("save_slot_empty"),
                             card_x + 20.0f, y + 50.0f,
                             TEXT_SIZE_LARGE,
                             color_rgba(0.5f, 0.48f, 0.44f, 0.7f),
                             TEXT_ALIGN_LEFT);
        }
    }

    /* Back hint */
    text_render_draw("[Esc] Back", cx, vh - 50.0f, TEXT_SIZE_BODY,
                     color_rgba(0.4f, 0.38f, 0.35f, 0.8f),
                     TEXT_ALIGN_CENTER);

    /* Delete confirmation overlay */
    if (ss->mode == SS_MODE_CONFIRM_DELETE) {
        /* Dim background */
        ui_draw_rect(0, 0, (float)vw, (float)vh, color_rgba(0, 0, 0, 0.6f));

        /* Dialog box */
        float dw = 400.0f, dh = 150.0f;
        float dx = cx - dw * 0.5f;
        float dy = vh * 0.5f - dh * 0.5f;
        ui_draw_rect_rounded(dx, dy, dw, dh, 10.0f, color_hex(0x1E1E22));

        /* Question */
        text_render_draw(strings_get("save_delete_confirm"),
                         cx, dy + 30.0f, TEXT_SIZE_LARGE,
                         color_rgba(0.9f, 0.85f, 0.70f, 1.0f),
                         TEXT_ALIGN_CENTER);

        /* Buttons */
        float btn_w = 140.0f, btn_h = 40.0f;
        float btn_y = dy + dh - 60.0f;
        float gap = 20.0f;

        /* Yes button */
        float yes_x = cx - btn_w - gap * 0.5f;
        Color yes_bg = (ss->delete_choice == 0)
            ? color_rgba(0.7f, 0.25f, 0.25f, 1.0f)
            : color_rgba(0.3f, 0.15f, 0.15f, 1.0f);
        ui_draw_rect_rounded(yes_x, btn_y, btn_w, btn_h, 6.0f, yes_bg);
        text_render_draw(strings_get("save_delete_yes"),
                         yes_x + btn_w * 0.5f, btn_y + 8.0f,
                         TEXT_SIZE_BODY, COLOR_WHITE, TEXT_ALIGN_CENTER);

        /* No button */
        float no_x = cx + gap * 0.5f;
        Color no_bg = (ss->delete_choice == 1)
            ? color_rgba(0.3f, 0.3f, 0.35f, 1.0f)
            : color_rgba(0.18f, 0.18f, 0.20f, 1.0f);
        ui_draw_rect_rounded(no_x, btn_y, btn_w, btn_h, 6.0f, no_bg);
        text_render_draw(strings_get("save_delete_no"),
                         no_x + btn_w * 0.5f, btn_y + 8.0f,
                         TEXT_SIZE_BODY, COLOR_WHITE, TEXT_ALIGN_CENTER);
    }
}

static void ss_destroy(State* self) {
    free(self);
}

/* ---------- Create ---------- */

State* save_select_scene_create(void) {
    SaveSelectScene* ss = (SaveSelectScene*)calloc(1, sizeof(SaveSelectScene));

    ss->base.on_enter      = ss_on_enter;
    ss->base.on_exit       = ss_on_exit;
    ss->base.on_pause      = NULL;
    ss->base.on_resume     = ss_on_enter; /* reload slots when returning */
    ss->base.handle_action = ss_handle_action;
    ss->base.update        = ss_update;
    ss->base.render        = ss_render;
    ss->base.destroy       = ss_destroy;
    ss->base.transparent   = false;

    return (State*)ss;
}
