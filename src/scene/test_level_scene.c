#include "scene/test_level_scene.h"
#include "scene/puzzle_scene.h"
#include "engine/engine.h"
#include "engine/utils.h"
#include "render/renderer.h"
#include "render/ui_draw.h"
#include "render/text_render.h"
#include "input/input_manager.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

/* ---------- Level entry ---------- */
#define MAX_LEVELS 64
#define MAX_NAME_LEN 64
#define MAX_PATH_LEN 512

typedef struct {
    char name[MAX_NAME_LEN];   /* display name (from filename) */
    char path[MAX_PATH_LEN];   /* full path to JSON file */
} LevelEntry;

typedef struct {
    State       base;
    int         selected;
    int         scroll_offset;  /* first visible item index */
    float       time;
    LevelEntry  levels[MAX_LEVELS];
    int         level_count;
} TestLevelScene;

/* ---------- Helpers ---------- */

/* How many items fit on screen (excluding header/footer) */
static int visible_item_count(int vh) {
    float usable = vh * 0.65f;  /* area for list items */
    float spacing = 42.0f;
    int count = (int)(usable / spacing);
    if (count < 3) count = 3;
    return count;
}

/* Convert filename like "test-pressure-plate.json" to "Pressure Plate" */
static void filename_to_display_name(const char* filename, char* out, int out_size) {
    /* Strip .json extension and any prefix like "test-" or "tutorial-" */
    char buf[MAX_NAME_LEN];
    strncpy(buf, filename, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Remove .json */
    char* dot = strrchr(buf, '.');
    if (dot) *dot = '\0';

    /* Copy to output, replacing hyphens/underscores with spaces and capitalizing words */
    int j = 0;
    bool cap_next = true;
    for (int i = 0; buf[i] && j < out_size - 1; i++) {
        if (buf[i] == '-' || buf[i] == '_') {
            out[j++] = ' ';
            cap_next = true;
        } else if (cap_next) {
            out[j++] = (buf[i] >= 'a' && buf[i] <= 'z')
                ? (char)(buf[i] - 32) : buf[i];
            cap_next = false;
        } else {
            out[j++] = buf[i];
        }
    }
    out[j] = '\0';
}

/* Scan a subdirectory of assets/levels/ for .json files */
static void scan_level_dir(TestLevelScene* tls, const char* subdir) {
    char dir_path[MAX_PATH_LEN];
    snprintf(dir_path, sizeof(dir_path), "%s/assets/levels/%s",
             platform_get_asset_dir(), subdir);

    DIR* dir = opendir(dir_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && tls->level_count < MAX_LEVELS) {
        const char* name = entry->d_name;
        size_t len = strlen(name);
        if (len < 6) continue;  /* minimum: "x.json" */
        if (strcmp(name + len - 5, ".json") != 0) continue;

        LevelEntry* le = &tls->levels[tls->level_count];
        snprintf(le->path, sizeof(le->path), "%s/assets/levels/%s/%s",
                 platform_get_asset_dir(), subdir, name);

        /* Display name: "Category: Name" */
        char base_name[MAX_NAME_LEN];
        filename_to_display_name(name, base_name, sizeof(base_name));

        /* Capitalize subdir for category prefix */
        char cat[MAX_NAME_LEN];
        filename_to_display_name(subdir, cat, sizeof(cat));

        snprintf(le->name, sizeof(le->name), "%s: %s", cat, base_name);
        tls->level_count++;
    }
    closedir(dir);
}

/* Compare level entries by name for sorting */
static int compare_levels(const void* a, const void* b) {
    const LevelEntry* la = (const LevelEntry*)a;
    const LevelEntry* lb = (const LevelEntry*)b;
    return strcmp(la->name, lb->name);
}

static void scan_all_levels(TestLevelScene* tls) {
    tls->level_count = 0;

    /* Scan known subdirectories */
    char levels_root[MAX_PATH_LEN];
    snprintf(levels_root, sizeof(levels_root), "%s/assets/levels",
             platform_get_asset_dir());

    DIR* root = opendir(levels_root);
    if (!root) {
        LOG_ERROR("Could not open levels directory: %s", levels_root);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(root)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        /* Check if it's a directory */
        char subpath[MAX_PATH_LEN];
        snprintf(subpath, sizeof(subpath), "%s/%s", levels_root, entry->d_name);
        struct stat st;
        if (stat(subpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            scan_level_dir(tls, entry->d_name);
        }
    }
    closedir(root);

    /* Sort alphabetically */
    if (tls->level_count > 1) {
        qsort(tls->levels, tls->level_count, sizeof(LevelEntry), compare_levels);
    }

    LOG_INFO("Test level scene: found %d levels", tls->level_count);
}

/* ---------- State callbacks ---------- */

static void tls_on_enter(State* self) {
    TestLevelScene* tls = (TestLevelScene*)self;
    tls->selected = 0;
    tls->scroll_offset = 0;
    tls->time = 0.0f;
    scan_all_levels(tls);
    input_manager_set_context(INPUT_CONTEXT_MENU);
    LOG_INFO("Test level scene entered");
}

static void tls_on_exit(State* self) {
    (void)self;
}

static void tls_on_resume(State* self) {
    TestLevelScene* tls = (TestLevelScene*)self;
    input_manager_set_context(INPUT_CONTEXT_MENU);
    /* Re-scan in case levels changed (unlikely but safe) */
    int prev_selected = tls->selected;
    scan_all_levels(tls);
    if (prev_selected < tls->level_count) {
        tls->selected = prev_selected;
    } else {
        tls->selected = 0;
    }
}

static void tls_handle_action(State* self, SemanticAction action) {
    TestLevelScene* tls = (TestLevelScene*)self;

    switch (action) {
    case ACTION_UI_UP:
        tls->selected--;
        if (tls->selected < 0) tls->selected = tls->level_count - 1;
        break;

    case ACTION_UI_DOWN:
        tls->selected++;
        if (tls->selected >= tls->level_count) tls->selected = 0;
        break;

    case ACTION_UI_CONFIRM:
        if (tls->level_count > 0) {
            LOG_INFO("Loading test level: %s", tls->levels[tls->selected].path);
            engine_push_state(puzzle_scene_create(tls->levels[tls->selected].path));
        }
        break;

    case ACTION_UI_BACK:
        engine_pop_state();
        break;

    default:
        break;
    }
}

static void tls_update(State* self, float dt) {
    TestLevelScene* tls = (TestLevelScene*)self;
    tls->time += dt;

    /* Keep selected item in view */
    int vw, vh;
    renderer_get_viewport(&vw, &vh);
    int max_visible = visible_item_count(vh);

    if (tls->selected < tls->scroll_offset) {
        tls->scroll_offset = tls->selected;
    } else if (tls->selected >= tls->scroll_offset + max_visible) {
        tls->scroll_offset = tls->selected - max_visible + 1;
    }
}

static void tls_render(State* self) {
    TestLevelScene* tls = (TestLevelScene*)self;
    int vw, vh;
    renderer_get_viewport(&vw, &vh);

    float cx = vw * 0.5f;

    /* Background */
    renderer_clear(color_hex(0x121218));

    /* Title */
    text_render_draw("Test Levels",
                     cx, vh * 0.06f, TEXT_SIZE_TITLE,
                     color_rgba(0.85f, 0.78f, 0.60f, 1.0f),
                     TEXT_ALIGN_CENTER);

    /* Level count subtitle */
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "%d levels found", tls->level_count);
    text_render_draw(subtitle, cx, vh * 0.06f + 52.0f, TEXT_SIZE_SMALL,
                     color_rgba(0.5f, 0.48f, 0.42f, 1.0f),
                     TEXT_ALIGN_CENTER);

    if (tls->level_count == 0) {
        text_render_draw("No levels found",
                         cx, vh * 0.5f, TEXT_SIZE_LARGE,
                         color_rgba(0.6f, 0.58f, 0.52f, 1.0f),
                         TEXT_ALIGN_CENTER);
    } else {
        /* Level list */
        int max_visible = visible_item_count(vh);
        float list_y = vh * 0.18f;
        float item_spacing = 42.0f;
        float item_w = fminf(600.0f, vw * 0.85f);
        float item_x = cx - item_w * 0.5f;

        for (int vi = 0; vi < max_visible && (tls->scroll_offset + vi) < tls->level_count; vi++) {
            int idx = tls->scroll_offset + vi;
            float y = list_y + vi * item_spacing;
            bool selected = (idx == tls->selected);

            if (selected) {
                float pulse = 0.8f + 0.2f * sinf(tls->time * 4.0f);
                /* Highlight background */
                ui_draw_rect_rounded(item_x, y - 4.0f,
                                     item_w, 36.0f, 6.0f,
                                     color_rgba(0.85f, 0.78f, 0.60f, 0.12f * pulse));

                /* Arrow indicator */
                text_render_draw("\xE2\x96\xB6", item_x + 12.0f, y + 2.0f,
                                 TEXT_SIZE_SMALL,
                                 color_rgba(0.95f, 0.88f, 0.65f, pulse),
                                 TEXT_ALIGN_LEFT);

                /* Selected text */
                text_render_draw(tls->levels[idx].name,
                                 item_x + 32.0f, y + 2.0f, TEXT_SIZE_BODY,
                                 color_rgba(0.95f, 0.88f, 0.65f, 1.0f),
                                 TEXT_ALIGN_LEFT);
            } else {
                text_render_draw(tls->levels[idx].name,
                                 item_x + 32.0f, y + 2.0f, TEXT_SIZE_BODY,
                                 color_rgba(0.6f, 0.58f, 0.52f, 0.9f),
                                 TEXT_ALIGN_LEFT);
            }
        }

        /* Scroll indicators */
        if (tls->scroll_offset > 0) {
            text_render_draw("\xE2\x96\xB2", cx, list_y - 18.0f,
                             TEXT_SIZE_SMALL,
                             color_rgba(0.6f, 0.58f, 0.52f, 0.6f),
                             TEXT_ALIGN_CENTER);
        }
        if (tls->scroll_offset + max_visible < tls->level_count) {
            float bottom_y = list_y + max_visible * item_spacing;
            text_render_draw("\xE2\x96\xBC", cx, bottom_y,
                             TEXT_SIZE_SMALL,
                             color_rgba(0.6f, 0.58f, 0.52f, 0.6f),
                             TEXT_ALIGN_CENTER);
        }
    }

    /* Footer */
    text_render_draw("[Esc] Back   [Enter] Play", cx, vh - 40.0f,
                     TEXT_SIZE_SMALL,
                     color_rgba(0.4f, 0.38f, 0.35f, 0.8f),
                     TEXT_ALIGN_CENTER);
}

static void tls_destroy(State* self) {
    free(self);
}

/* ---------- Create ---------- */

State* test_level_scene_create(void) {
    TestLevelScene* tls = (TestLevelScene*)calloc(1, sizeof(TestLevelScene));

    tls->base.on_enter      = tls_on_enter;
    tls->base.on_exit       = tls_on_exit;
    tls->base.on_pause      = NULL;
    tls->base.on_resume     = tls_on_resume;
    tls->base.handle_action = tls_handle_action;
    tls->base.update        = tls_update;
    tls->base.render        = tls_render;
    tls->base.destroy       = tls_destroy;
    tls->base.transparent   = false;

    return (State*)tls;
}
