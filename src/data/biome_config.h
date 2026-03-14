#ifndef BIOME_CONFIG_H
#define BIOME_CONFIG_H

#include <stdbool.h>

/*
 * Biome configuration — defines the visual style and procedural generation
 * parameters for a biome's diorama. Loaded from JSON at level load time.
 *
 * Each biome controls: color palette, wall block composition style,
 * decoration density/prefabs, lantern placement, platform depth,
 * back wall height, door framing, and edge border style.
 */

#define BIOME_MAX_PREFABS          32
#define BIOME_MAX_PREFAB_BOXES     24
#define BIOME_MAX_PREFAB_NAME      32
#define BIOME_MAX_DECORATION_REFS  8

/* A single box within a prefab definition */
typedef struct {
    float dx, dy, dz;      /* offset from placement origin */
    float sx, sy, sz;       /* size */
    float r, g, b, a;       /* color */
    bool  no_cull;
} PrefabBox;

/* A named prefab voxel cluster */
typedef struct {
    char      name[BIOME_MAX_PREFAB_NAME];
    PrefabBox boxes[BIOME_MAX_PREFAB_BOXES];
    int       box_count;
} BiomePrefab;

/* Biome color palette */
typedef struct {
    float floor_a[3];        /* checkerboard color A */
    float floor_b[3];        /* checkerboard color B */
    float wall[3];           /* base wall color */
    float wall_jitter;       /* max per-channel wall color variation */
    float accent[3];         /* accent color (doors, trim) */
    float impassable[3];     /* impassable tile fill color */
    float platform_side[3];  /* platform side face color */
    float back_wall[3];      /* back wall base color */
} BiomePalette;

/* Wall surface texture style — controls the procedural stone/brick/wood/metal
 * pattern rendered on wall faces in the fragment shader. All visual detail
 * is shader-based; walls are single prisms in geometry. */
typedef struct {
    float stone_height;        /* row height in world units (0.09 = ~3 rows on wall) */
    float stone_width_min;     /* min stone width in world units */
    float stone_width_max;     /* max stone width in world units */
    float mortar_width;        /* mortar line thickness as fraction of stone (0=none, 0.06=thin) */
    float mortar_darkness;     /* mortar darkening (0=invisible, 1=black). 0.50 = half-dark */
    float bevel_width;         /* rounded-edge gradient width as fraction of stone (0=sharp, 0.3=very round) */
    float bevel_darkness;      /* bevel edge darkening (0=none, 1=black). 0.12 = subtle */
    float color_variation;     /* per-stone color variation magnitude (0=uniform, 0.20=strong) */
    float grain_intensity;     /* surface FBM noise magnitude (0=smooth, 0.10=strong) */
    float grain_scale;         /* surface FBM frequency multiplier (10=fine stone, 4=coarse wood) */
    float wear;                /* edge wear amount (0=pristine straight edges, 1=heavily worn/chipped) */
    float gap_color[3];        /* RGB color blended into mortar/bevel gaps (default 0,0,0 = black) */
} WallStyle;

/* Decoration layer configuration */
typedef struct {
    float density;             /* 0.0-1.0 chance per eligible position */
    int   max_per_tile;        /* max decorations per tile */
    char  prefab_names[BIOME_MAX_DECORATION_REFS][BIOME_MAX_PREFAB_NAME];
    int   prefab_count;
} DecorationLayer;

/* Lantern pillar configuration */
typedef struct {
    float glow_color[3];       /* point light color */
    float density;             /* controls placement frequency */
    bool  place_at_corners;    /* place at diorama corners */
    bool  place_at_wall_ends;  /* place at wall segment endpoints */
} LanternConfig;

/* Platform configuration */
typedef struct {
    int depth_blocks;          /* depth in block layers (2-4) */
} PlatformConfig;

/* Back wall configuration */
typedef struct {
    float height_multiplier;   /* relative to standard wall height */
    float decoration_density;  /* decoration density on back wall */
} BackWallConfig;

/* Edge border configuration */
typedef struct {
    int depth;                 /* border width in half-tiles */
} EdgeBorderConfig;

/* Floor tile sub-voxel style */
typedef struct {
    int   subdivisions;       /* NxN sub-stones per tile (2-4) */
    float regularity;         /* 0=very irregular sizes, 1=uniform grid */
    float edge_inset;         /* mortar/gap at logical tile boundary */
    float inner_gap;          /* gap between sub-voxels within a tile */
    float height_variation;   /* max random height offset per sub-voxel */
    float color_jitter;       /* per-sub-voxel color variation (within same tone) */
    float size_jitter;        /* random size variation per sub-voxel (0..1) */
} FloorStyle;

/* Floor shadow lightmap configuration (per-biome tuning)
 *
 * shadow_softness is the UNIVERSAL shadow softness control (0.0 = hard,
 * 1.0 = very soft). All shadow edge gradients derive from this single
 * value, so changing it tunes the overall shadow feel in one place:
 *   - Floor lightmap blur: actual_blur = shadow_softness * shadow_blur_radius
 *   - Wall seam gradient width: narrow at 0, wide at 1
 *   - Wall seam/top darkening amount: darker at 0, lighter at 1
 */
typedef struct {
    float shadow_softness;    /* universal softness 0..1 (0=hard, 1=soft), default 0.4 */
    float shadow_scale;       /* footprint multiplier (>1 = shadows larger than walls), default 1.3 */
    float shadow_offset_x;    /* world offset X (simulates light angle), default 0.05 */
    float shadow_offset_z;    /* world offset Z, default -0.05 */
    float shadow_blur_radius; /* max blur radius in texels (scaled by shadow_softness), default 6.0 */
    float shadow_intensity;   /* max darkness 0..1, default 0.55 */
    int   shadow_resolution;  /* texels per tile, default 32 */
} FloorShadowConfig;

/* Actor shadow configuration (per-biome, independent of floor shadows).
 * Controls the blurred rectangular shadow rendered under Theseus/Minotaur.
 * Same parameter semantics as FloorShadowConfig but tuned separately. */
typedef struct {
    float shadow_softness;    /* blur softness 0..1 (0=hard, 1=soft), default 0.4 */
    float shadow_scale;       /* footprint multiplier (>1 = wider shadow), default 1.3 */
    float shadow_offset_x;    /* world offset X (simulates light angle), default 0.05 */
    float shadow_offset_z;    /* world offset Z, default -0.05 */
    float shadow_blur_radius; /* max blur radius in texels (scaled by softness), default 6.0 */
    float shadow_intensity;   /* max darkness 0..1, default 0.55 */
    int   shadow_resolution;  /* reference resolution for blur conversion, default 32 */
} ActorShadowConfig;

/* Groove trench configuration — controls the visual channel cut into
 * floor tiles where groove boxes can slide. */
typedef struct {
    float trench_depth;        /* depth of groove channel (default 0.07) */
    float trench_inset;        /* inset from tile edge to trench wall (default 0.12) */
    float cap_inset;           /* endpoint cap inset from tile edge (default 0.15) */
    float color_darken;        /* darken factor for trench floor (default 0.80) */
    float wall_darken;         /* darken factor for trench inner walls (default 0.70) */
} GrooveTrenchConfig;

/* Door style configuration */
typedef struct {
    int frame_height_blocks;   /* door frame height in blocks */
} DoorConfig;

/* Top-level biome configuration */
typedef struct {
    char             id[64];
    char             name[64];
    BiomePalette     palette;
    WallStyle        wall_style;
    FloorStyle       floor_style;
    DecorationLayer  floor_decorations;
    DecorationLayer  wall_decorations;
    LanternConfig    lanterns;
    PlatformConfig   platform;
    BackWallConfig   back_wall;
    EdgeBorderConfig edge_border;
    DoorConfig       doors;
    FloorShadowConfig  floor_shadow;
    ActorShadowConfig  actor_shadow;
    GrooveTrenchConfig groove_trench;

    /* Prefab library (referenced by name from decoration layers) */
    BiomePrefab      prefabs[BIOME_MAX_PREFABS];
    int              prefab_count;
} BiomeConfig;

/* Fill config with sensible stone-labyrinth defaults. */
void biome_config_defaults(BiomeConfig* cfg);

/* Load biome config from JSON file. Falls back to defaults on error.
 * Returns true on success. */
bool biome_config_load(BiomeConfig* cfg, const char* json_path);

/* Find a prefab by name. Returns NULL if not found. */
const BiomePrefab* biome_config_find_prefab(const BiomeConfig* cfg, const char* name);

#endif /* BIOME_CONFIG_H */
