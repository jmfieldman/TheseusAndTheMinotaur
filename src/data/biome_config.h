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

/* Wall block composition style */
typedef struct {
    int   blocks_per_segment;  /* blocks laid end-to-end per wall tile (3-5) */
    int   rows_of_blocks;      /* vertical layers (2-3) */
    float block_regularity;    /* 0=very irregular, 1=perfect grid */
    float roughness;           /* position jitter magnitude */
    float height_variation;    /* top-row random height offset */
    float color_jitter;        /* per-block color variation */
    float mortar_gap;          /* gap between blocks in world units */
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
