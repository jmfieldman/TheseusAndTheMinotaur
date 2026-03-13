#include "level_loader.h"
#include "../engine/utils.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Feature factory registry ──────────────────────────── */

#define MAX_FEATURE_FACTORIES 32

typedef struct {
    char name[64];
    FeatureFactory factory;
} FeatureFactoryEntry;

static FeatureFactoryEntry s_factories[MAX_FEATURE_FACTORIES];
static int s_factory_count = 0;

void level_loader_register_feature(const char* type_name,
                                   FeatureFactory factory) {
    if (s_factory_count >= MAX_FEATURE_FACTORIES) {
        LOG_ERROR("level_loader: factory registry full");
        return;
    }
    strncpy(s_factories[s_factory_count].name, type_name, 63);
    s_factories[s_factory_count].name[63] = '\0';
    s_factories[s_factory_count].factory = factory;
    s_factory_count++;
    LOG_DEBUG("level_loader: registered feature factory '%s'", type_name);
}

static FeatureFactory find_factory(const char* type_name) {
    for (int i = 0; i < s_factory_count; i++) {
        if (strcmp(s_factories[i].name, type_name) == 0) {
            return s_factories[i].factory;
        }
    }
    return NULL;
}

/* ── Helpers ───────────────────────────────────────────── */

static Direction parse_side(const char* side_str) {
    if (!side_str) return DIR_NONE;
    if (strcmp(side_str, "north") == 0) return DIR_NORTH;
    if (strcmp(side_str, "south") == 0) return DIR_SOUTH;
    if (strcmp(side_str, "east")  == 0) return DIR_EAST;
    if (strcmp(side_str, "west")  == 0) return DIR_WEST;
    LOG_WARN("level_loader: unknown side '%s'", side_str);
    return DIR_NONE;
}

static bool parse_int(const cJSON* obj, const char* key, int* out) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item)) return false;
    *out = item->valueint;
    return true;
}

static bool parse_string(const cJSON* obj, const char* key,
                         char* out, int out_size) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || !item->valuestring) return false;
    strncpy(out, item->valuestring, (size_t)(out_size - 1));
    out[out_size - 1] = '\0';
    return true;
}

/* ── Core loader ───────────────────────────────────────── */

static Grid* load_from_json(cJSON* root) {
    if (!root) {
        LOG_ERROR("level_loader: NULL JSON root");
        return NULL;
    }

    /* ── Grid dimensions ── */
    const cJSON* grid_obj = cJSON_GetObjectItemCaseSensitive(root, "grid");
    if (!grid_obj) {
        LOG_ERROR("level_loader: missing 'grid' object");
        return NULL;
    }

    int width = 0, height = 0;
    if (!parse_int(grid_obj, "width", &width) ||
        !parse_int(grid_obj, "height", &height)) {
        LOG_ERROR("level_loader: missing grid width/height");
        return NULL;
    }

    Grid* grid = grid_create(width, height);
    if (!grid) return NULL;

    /* ── Metadata ── */
    parse_string(root, "id",   grid->level_id,   sizeof(grid->level_id));
    parse_string(root, "name", grid->level_name,  sizeof(grid->level_name));
    parse_string(root, "biome", grid->biome,      sizeof(grid->biome));
    parse_int(root, "optimal_turns", &grid->optimal_turns);

    /* ── Entity positions ── */
    const cJSON* theseus_obj = cJSON_GetObjectItemCaseSensitive(root, "theseus");
    if (theseus_obj) {
        parse_int(theseus_obj, "col", &grid->theseus_col);
        parse_int(theseus_obj, "row", &grid->theseus_row);
    }

    const cJSON* minotaur_obj = cJSON_GetObjectItemCaseSensitive(root, "minotaur");
    if (minotaur_obj) {
        parse_int(minotaur_obj, "col", &grid->minotaur_col);
        parse_int(minotaur_obj, "row", &grid->minotaur_row);
    }

    /* ── Entrance door ── */
    const cJSON* entrance_obj = cJSON_GetObjectItemCaseSensitive(root, "entrance");
    if (entrance_obj) {
        parse_int(entrance_obj, "col", &grid->entrance_col);
        parse_int(entrance_obj, "row", &grid->entrance_row);
        char side_str[16] = {0};
        parse_string(entrance_obj, "side", side_str, sizeof(side_str));
        grid->entrance_side = parse_side(side_str);

        /* Remove the boundary wall at the entrance so Theseus can enter.
         * The entrance closes immediately, so we re-seal it after the
         * initial state is captured. For now, the entrance is just
         * metadata — Theseus starts inside the grid. */
    }

    /* ── Exit door ── */
    const cJSON* exit_obj = cJSON_GetObjectItemCaseSensitive(root, "exit");
    if (exit_obj) {
        parse_int(exit_obj, "col", &grid->exit_col);
        parse_int(exit_obj, "row", &grid->exit_row);
        char side_str[16] = {0};
        parse_string(exit_obj, "side", side_str, sizeof(side_str));
        grid->exit_side = parse_side(side_str);

        /* Remove the boundary wall at the exit so Theseus can leave.
         * Note: grid_can_move checks walls, so we clear the exit wall
         * to allow passage.  The Minotaur is prevented from exiting
         * by the is_exit_move check in minotaur.c. */
        grid_set_wall(grid, grid->exit_col, grid->exit_row,
                      grid->exit_side, false);
    }

    /* ── Interior walls ── */
    const cJSON* walls_arr = cJSON_GetObjectItemCaseSensitive(root, "walls");
    if (cJSON_IsArray(walls_arr)) {
        const cJSON* wall = NULL;
        cJSON_ArrayForEach(wall, walls_arr) {
            int col = 0, row = 0;
            char side_str[16] = {0};
            parse_int(wall, "col", &col);
            parse_int(wall, "row", &row);
            parse_string(wall, "side", side_str, sizeof(side_str));
            Direction side = parse_side(side_str);
            if (side != DIR_NONE) {
                grid_set_wall(grid, col, row, side, true);
            }
        }
    }

    /* ── Impassable tiles ── */
    const cJSON* imp_arr = cJSON_GetObjectItemCaseSensitive(root, "impassable");
    if (cJSON_IsArray(imp_arr)) {
        const cJSON* imp = NULL;
        cJSON_ArrayForEach(imp, imp_arr) {
            int col = 0, row = 0;
            parse_int(imp, "col", &col);
            parse_int(imp, "row", &row);
            Cell* cell = grid_cell(grid, col, row);
            if (cell) {
                cell->impassable = true;
            }
        }
    }

    /* ── Features ── */
    const cJSON* feat_arr = cJSON_GetObjectItemCaseSensitive(root, "features");
    if (cJSON_IsArray(feat_arr)) {
        const cJSON* feat = NULL;
        cJSON_ArrayForEach(feat, feat_arr) {
            const cJSON* type_item = cJSON_GetObjectItemCaseSensitive(feat, "type");
            if (!cJSON_IsString(type_item)) {
                LOG_WARN("level_loader: feature missing 'type'");
                continue;
            }
            const char* type_name = type_item->valuestring;

            const cJSON* pos_obj = cJSON_GetObjectItemCaseSensitive(feat, "position");
            int col = 0, row = 0;
            if (pos_obj) {
                parse_int(pos_obj, "col", &col);
                parse_int(pos_obj, "row", &row);
            }

            const cJSON* config = cJSON_GetObjectItemCaseSensitive(feat, "config");

            FeatureFactory factory = find_factory(type_name);
            if (!factory) {
                LOG_WARN("level_loader: no factory for feature type '%s'",
                         type_name);
                continue;
            }

            Feature* feature = factory(col, row, config);
            if (feature) {
                grid_add_feature(grid, feature);
            } else {
                LOG_WARN("level_loader: factory failed for '%s' at (%d,%d)",
                         type_name, col, row);
            }
        }
    }

    LOG_INFO("level_loader: loaded '%s' (%s) %dx%d, %d walls, %d features",
             grid->level_id, grid->level_name,
             grid->cols, grid->rows,
             /* count internal walls (rough) */
             cJSON_IsArray(walls_arr) ? cJSON_GetArraySize(walls_arr) : 0,
             grid->feature_count);

    return grid;
}

/* ── Public API ────────────────────────────────────────── */

Grid* level_load_from_string(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        LOG_ERROR("level_loader: JSON parse error: %s",
                  cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown");
        return NULL;
    }

    Grid* grid = load_from_json(root);
    cJSON_Delete(root);
    return grid;
}

Grid* level_load_from_file(const char* json_path) {
    FILE* fp = fopen(json_path, "rb");
    if (!fp) {
        LOG_ERROR("level_loader: cannot open '%s'", json_path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {  /* 1 MB sanity limit */
        LOG_ERROR("level_loader: file '%s' has invalid size %ld", json_path, size);
        fclose(fp);
        return NULL;
    }

    char* buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    buf[read] = '\0';

    Grid* grid = level_load_from_string(buf);
    free(buf);
    return grid;
}
