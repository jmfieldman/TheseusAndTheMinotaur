#include "data/biome_config.h"
#include "engine/utils.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- JSON helpers ---------- */

static void parse_color3(const cJSON* arr, float out[3]) {
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) < 3) return;
    out[0] = (float)cJSON_GetArrayItem(arr, 0)->valuedouble;
    out[1] = (float)cJSON_GetArrayItem(arr, 1)->valuedouble;
    out[2] = (float)cJSON_GetArrayItem(arr, 2)->valuedouble;
}

static float parse_float(const cJSON* obj, const char* key, float def) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item && cJSON_IsNumber(item)) return (float)item->valuedouble;
    return def;
}

static int parse_int(const cJSON* obj, const char* key, int def) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item && cJSON_IsNumber(item)) return item->valueint;
    return def;
}

static bool parse_bool(const cJSON* obj, const char* key, bool def) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item) {
        if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
        if (cJSON_IsNumber(item)) return item->valueint != 0;
    }
    return def;
}

static void parse_string(const cJSON* obj, const char* key, char* out, int max_len) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item && cJSON_IsString(item)) {
        strncpy(out, item->valuestring, max_len - 1);
        out[max_len - 1] = '\0';
    }
}

/* ---------- Section parsers ---------- */

static void parse_palette(const cJSON* obj, BiomePalette* pal) {
    if (!obj) return;
    const cJSON* item;
    if ((item = cJSON_GetObjectItemCaseSensitive(obj, "floor_a"))) parse_color3(item, pal->floor_a);
    if ((item = cJSON_GetObjectItemCaseSensitive(obj, "floor_b"))) parse_color3(item, pal->floor_b);
    if ((item = cJSON_GetObjectItemCaseSensitive(obj, "wall"))) parse_color3(item, pal->wall);
    pal->wall_jitter = parse_float(obj, "wall_jitter", pal->wall_jitter);
    if ((item = cJSON_GetObjectItemCaseSensitive(obj, "accent"))) parse_color3(item, pal->accent);
    if ((item = cJSON_GetObjectItemCaseSensitive(obj, "impassable"))) parse_color3(item, pal->impassable);
    if ((item = cJSON_GetObjectItemCaseSensitive(obj, "platform_side"))) parse_color3(item, pal->platform_side);
    if ((item = cJSON_GetObjectItemCaseSensitive(obj, "back_wall"))) parse_color3(item, pal->back_wall);
}

static void parse_wall_style(const cJSON* obj, WallStyle* ws) {
    if (!obj) return;
    ws->stone_height = parse_float(obj, "stone_height", ws->stone_height);
    ws->stone_width_min = parse_float(obj, "stone_width_min", ws->stone_width_min);
    ws->stone_width_max = parse_float(obj, "stone_width_max", ws->stone_width_max);
    ws->mortar_width = parse_float(obj, "mortar_width", ws->mortar_width);
    ws->mortar_darkness = parse_float(obj, "mortar_darkness", ws->mortar_darkness);
    ws->bevel_width = parse_float(obj, "bevel_width", ws->bevel_width);
    ws->bevel_darkness = parse_float(obj, "bevel_darkness", ws->bevel_darkness);
    ws->color_variation = parse_float(obj, "color_variation", ws->color_variation);
    ws->grain_intensity = parse_float(obj, "grain_intensity", ws->grain_intensity);
    ws->grain_scale = parse_float(obj, "grain_scale", ws->grain_scale);
    ws->wear = parse_float(obj, "wear", ws->wear);
}

static void parse_floor_style(const cJSON* obj, FloorStyle* fs) {
    if (!obj) return;
    fs->subdivisions = parse_int(obj, "subdivisions", fs->subdivisions);
    fs->regularity = parse_float(obj, "regularity", fs->regularity);
    fs->edge_inset = parse_float(obj, "edge_inset", fs->edge_inset);
    fs->inner_gap = parse_float(obj, "inner_gap", fs->inner_gap);
    fs->height_variation = parse_float(obj, "height_variation", fs->height_variation);
    fs->color_jitter = parse_float(obj, "color_jitter", fs->color_jitter);
    fs->size_jitter = parse_float(obj, "size_jitter", fs->size_jitter);
}

static void parse_decoration_layer(const cJSON* obj, DecorationLayer* dl) {
    if (!obj) return;
    dl->density = parse_float(obj, "density", dl->density);
    dl->max_per_tile = parse_int(obj, "max_per_tile", dl->max_per_tile);
    const cJSON* prefabs = cJSON_GetObjectItemCaseSensitive(obj, "prefabs");
    if (cJSON_IsArray(prefabs)) {
        dl->prefab_count = 0;
        const cJSON* p;
        cJSON_ArrayForEach(p, prefabs) {
            if (cJSON_IsString(p) && dl->prefab_count < BIOME_MAX_DECORATION_REFS) {
                strncpy(dl->prefab_names[dl->prefab_count], p->valuestring,
                        BIOME_MAX_PREFAB_NAME - 1);
                dl->prefab_names[dl->prefab_count][BIOME_MAX_PREFAB_NAME - 1] = '\0';
                dl->prefab_count++;
            }
        }
    }
}

static void parse_prefab(const cJSON* obj, BiomePrefab* prefab) {
    parse_string(obj, "name", prefab->name, BIOME_MAX_PREFAB_NAME);
    prefab->box_count = 0;

    const cJSON* boxes = cJSON_GetObjectItemCaseSensitive(obj, "boxes");
    if (!cJSON_IsArray(boxes)) return;

    const cJSON* box_json;
    cJSON_ArrayForEach(box_json, boxes) {
        if (prefab->box_count >= BIOME_MAX_PREFAB_BOXES) break;
        PrefabBox* box = &prefab->boxes[prefab->box_count++];
        memset(box, 0, sizeof(PrefabBox));

        box->dx = parse_float(box_json, "dx", 0.0f);
        box->dy = parse_float(box_json, "dy", 0.0f);
        box->dz = parse_float(box_json, "dz", 0.0f);
        box->sx = parse_float(box_json, "sx", 0.1f);
        box->sy = parse_float(box_json, "sy", 0.1f);
        box->sz = parse_float(box_json, "sz", 0.1f);
        box->no_cull = parse_bool(box_json, "no_cull", false);

        const cJSON* color = cJSON_GetObjectItemCaseSensitive(box_json, "color");
        if (cJSON_IsArray(color) && cJSON_GetArraySize(color) >= 3) {
            box->r = (float)cJSON_GetArrayItem(color, 0)->valuedouble;
            box->g = (float)cJSON_GetArrayItem(color, 1)->valuedouble;
            box->b = (float)cJSON_GetArrayItem(color, 2)->valuedouble;
            box->a = (cJSON_GetArraySize(color) >= 4)
                     ? (float)cJSON_GetArrayItem(color, 3)->valuedouble : 1.0f;
        } else {
            box->r = 0.5f; box->g = 0.5f; box->b = 0.5f; box->a = 1.0f;
        }
    }
}

/* ---------- Public API ---------- */

void biome_config_defaults(BiomeConfig* cfg) {
    memset(cfg, 0, sizeof(BiomeConfig));

    strncpy(cfg->id, "stone_labyrinth", sizeof(cfg->id) - 1);
    strncpy(cfg->name, "Stone Labyrinth", sizeof(cfg->name) - 1);

    /* Palette — warm stone */
    float fa[3] = {0.33f, 0.31f, 0.28f};
    float fb[3] = {0.44f, 0.42f, 0.38f};
    float w[3]  = {0.50f, 0.47f, 0.40f};
    float ac[3] = {0.75f, 0.65f, 0.25f};
    float im[3] = {0.25f, 0.23f, 0.20f};
    float ps[3] = {0.22f, 0.20f, 0.18f};
    float bw[3] = {0.45f, 0.42f, 0.36f};

    memcpy(cfg->palette.floor_a, fa, sizeof(fa));
    memcpy(cfg->palette.floor_b, fb, sizeof(fb));
    memcpy(cfg->palette.wall, w, sizeof(w));
    cfg->palette.wall_jitter = 0.06f;
    memcpy(cfg->palette.accent, ac, sizeof(ac));
    memcpy(cfg->palette.impassable, im, sizeof(im));
    memcpy(cfg->palette.platform_side, ps, sizeof(ps));
    memcpy(cfg->palette.back_wall, bw, sizeof(bw));

    /* Wall surface texture style — weathered stone */
    cfg->wall_style.stone_height = 0.09f;
    cfg->wall_style.stone_width_min = 0.15f;
    cfg->wall_style.stone_width_max = 0.30f;
    cfg->wall_style.mortar_width = 0.06f;
    cfg->wall_style.mortar_darkness = 0.50f;
    cfg->wall_style.bevel_width = 0.22f;
    cfg->wall_style.bevel_darkness = 0.12f;
    cfg->wall_style.color_variation = 0.20f;
    cfg->wall_style.grain_intensity = 0.10f;
    cfg->wall_style.grain_scale = 10.0f;
    cfg->wall_style.wear = 0.0f;

    /* Floor style */
    cfg->floor_style.subdivisions = 2;
    cfg->floor_style.regularity = 0.85f;
    cfg->floor_style.edge_inset = 0.0f;
    cfg->floor_style.inner_gap = 0.0f;
    cfg->floor_style.height_variation = 0.003f;
    cfg->floor_style.color_jitter = 0.01f;
    cfg->floor_style.size_jitter = 0.06f;

    /* Floor decorations */
    cfg->floor_decorations.density = 0.10f;
    cfg->floor_decorations.max_per_tile = 1;

    /* Wall decorations */
    cfg->wall_decorations.density = 0.15f;
    cfg->wall_decorations.max_per_tile = 1;

    /* Lanterns */
    cfg->lanterns.glow_color[0] = 0.4f;
    cfg->lanterns.glow_color[1] = 0.8f;
    cfg->lanterns.glow_color[2] = 0.8f;
    cfg->lanterns.density = 0.5f;
    cfg->lanterns.place_at_corners = true;
    cfg->lanterns.place_at_wall_ends = true;

    /* Platform */
    cfg->platform.depth_blocks = 3;

    /* Back wall */
    cfg->back_wall.height_multiplier = 4.0f;
    cfg->back_wall.decoration_density = 0.3f;

    /* Edge border */
    cfg->edge_border.depth = 1;

    /* Doors */
    cfg->doors.frame_height_blocks = 4;

    /* Floor shadow lightmap */
    cfg->floor_shadow.shadow_softness = 0.4f;
    cfg->floor_shadow.shadow_scale = 1.3f;
    cfg->floor_shadow.shadow_offset_x = 0.05f;
    cfg->floor_shadow.shadow_offset_z = -0.05f;
    cfg->floor_shadow.shadow_blur_radius = 6.0f;
    cfg->floor_shadow.shadow_intensity = 0.55f;
    cfg->floor_shadow.shadow_resolution = 32;
}

bool biome_config_load(BiomeConfig* cfg, const char* json_path) {
    FILE* f = fopen(json_path, "rb");
    if (!f) {
        LOG_WARN("biome_config: cannot open '%s', using defaults", json_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        LOG_ERROR("biome_config: failed to parse '%s': %s", json_path, cJSON_GetErrorPtr());
        return false;
    }

    /* Top-level fields */
    parse_string(root, "id", cfg->id, sizeof(cfg->id));
    parse_string(root, "name", cfg->name, sizeof(cfg->name));

    /* Sections */
    parse_palette(cJSON_GetObjectItemCaseSensitive(root, "palette"), &cfg->palette);
    parse_wall_style(cJSON_GetObjectItemCaseSensitive(root, "wall_style"), &cfg->wall_style);
    parse_floor_style(cJSON_GetObjectItemCaseSensitive(root, "floor_style"), &cfg->floor_style);
    parse_decoration_layer(cJSON_GetObjectItemCaseSensitive(root, "floor_decorations"),
                           &cfg->floor_decorations);
    parse_decoration_layer(cJSON_GetObjectItemCaseSensitive(root, "wall_decorations"),
                           &cfg->wall_decorations);

    /* Lanterns */
    const cJSON* lan = cJSON_GetObjectItemCaseSensitive(root, "lantern_pillars");
    if (lan) {
        const cJSON* gc = cJSON_GetObjectItemCaseSensitive(lan, "glow_color");
        if (gc) parse_color3(gc, cfg->lanterns.glow_color);
        cfg->lanterns.density = parse_float(lan, "density", cfg->lanterns.density);
        cfg->lanterns.place_at_corners = parse_bool(lan, "place_at_corners", cfg->lanterns.place_at_corners);
        cfg->lanterns.place_at_wall_ends = parse_bool(lan, "place_at_wall_endpoints", cfg->lanterns.place_at_wall_ends);
    }

    /* Platform */
    const cJSON* plat = cJSON_GetObjectItemCaseSensitive(root, "platform");
    if (plat) {
        cfg->platform.depth_blocks = parse_int(plat, "depth_blocks", cfg->platform.depth_blocks);
    }

    /* Back wall */
    const cJSON* bw = cJSON_GetObjectItemCaseSensitive(root, "back_wall");
    if (bw) {
        cfg->back_wall.height_multiplier = parse_float(bw, "height_multiplier",
                                                        cfg->back_wall.height_multiplier);
        cfg->back_wall.decoration_density = parse_float(bw, "decoration_density",
                                                         cfg->back_wall.decoration_density);
    }

    /* Edge border */
    const cJSON* eb = cJSON_GetObjectItemCaseSensitive(root, "edge_border");
    if (eb) {
        cfg->edge_border.depth = parse_int(eb, "depth", cfg->edge_border.depth);
    }

    /* Doors */
    const cJSON* doors = cJSON_GetObjectItemCaseSensitive(root, "doors");
    if (doors) {
        cfg->doors.frame_height_blocks = parse_int(doors, "frame_height_blocks",
                                                    cfg->doors.frame_height_blocks);
    }

    /* Floor shadow */
    const cJSON* fs = cJSON_GetObjectItemCaseSensitive(root, "floor_shadow");
    if (fs) {
        cfg->floor_shadow.shadow_softness = parse_float(fs, "shadow_softness",
                                                          cfg->floor_shadow.shadow_softness);
        cfg->floor_shadow.shadow_scale = parse_float(fs, "shadow_scale",
                                                      cfg->floor_shadow.shadow_scale);
        cfg->floor_shadow.shadow_offset_x = parse_float(fs, "shadow_offset_x",
                                                          cfg->floor_shadow.shadow_offset_x);
        cfg->floor_shadow.shadow_offset_z = parse_float(fs, "shadow_offset_z",
                                                          cfg->floor_shadow.shadow_offset_z);
        cfg->floor_shadow.shadow_blur_radius = parse_float(fs, "shadow_blur_radius",
                                                             cfg->floor_shadow.shadow_blur_radius);
        cfg->floor_shadow.shadow_intensity = parse_float(fs, "shadow_intensity",
                                                           cfg->floor_shadow.shadow_intensity);
        cfg->floor_shadow.shadow_resolution = parse_int(fs, "shadow_resolution",
                                                          cfg->floor_shadow.shadow_resolution);
    }

    /* Prefabs */
    const cJSON* prefabs = cJSON_GetObjectItemCaseSensitive(root, "prefabs");
    if (cJSON_IsArray(prefabs)) {
        cfg->prefab_count = 0;
        const cJSON* p;
        cJSON_ArrayForEach(p, prefabs) {
            if (cfg->prefab_count >= BIOME_MAX_PREFABS) break;
            parse_prefab(p, &cfg->prefabs[cfg->prefab_count++]);
        }
    }

    cJSON_Delete(root);
    LOG_INFO("biome_config: loaded '%s' (%s) with %d prefabs",
             cfg->id, json_path, cfg->prefab_count);
    return true;
}

const BiomePrefab* biome_config_find_prefab(const BiomeConfig* cfg, const char* name) {
    for (int i = 0; i < cfg->prefab_count; i++) {
        if (strcmp(cfg->prefabs[i].name, name) == 0) {
            return &cfg->prefabs[i];
        }
    }
    return NULL;
}
