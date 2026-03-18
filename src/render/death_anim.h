#ifndef DEATH_ANIM_H
#define DEATH_ANIM_H

#include <glad/gl.h>
#include <stdbool.h>

/*
 * Death animation system — voxel-decomposition death effects.
 *
 * When Theseus dies, his body mesh is decomposed into individual rigid voxel
 * particles that scatter with physics (gravity, floor-height awareness, pit
 * fall-through, wall collision).  Each particle is rendered as a colored box.
 *
 * Step 6.9a: Core framework with gravity + floor heights + pit fall-through.
 * Step 6.9b: Wall collision + keyframe-based undo reversal.
 */

/* Dependencies */
#include "render/actor_render.h"
#include "game/grid.h"
#include "data/biome_config.h"

/* Maximum death voxels per animation */
#define DEATH_VOXEL_MAX     64

/* Maximum keyframes per voxel (init + periodic samples + collisions + rest) */
#define VOXEL_KEYFRAME_MAX  32

/* Death type — determines scatter pattern (initialized in Steps 6.10–6.14) */
typedef enum {
    DEATH_SQUISH,       /* Minotaur rolls onto Theseus */
    DEATH_WALK_INTO,    /* Theseus walks into Minotaur */
    DEATH_SPIKE,        /* Spike trap impales Theseus */
    DEATH_PETRIFY,      /* Medusa petrification */
    DEATH_PIT_FALL,     /* Fall into pit */
    DEATH_GENERIC       /* Fallback scatter (used until specific types are implemented) */
} DeathType;

/* Keyframe snapshot of a voxel's state at a collision/event time */
typedef struct {
    float time;             /* Seconds since animation start */
    float pos[3];           /* Position at this moment */
    float vel[3];           /* Velocity at this moment */
    float rot[3];           /* Rotation at this moment */
    float angular_vel[3];   /* Angular velocity at this moment */
    float scale[3];         /* Scale at this moment */
    bool  fallen;           /* Whether the voxel had fallen at this point */
} VoxelKeyframe;

/* Single voxel particle in a death animation */
typedef struct {
    float pos[3];           /* Current world position */
    float vel[3];           /* Current velocity */
    float rot[3];           /* Current rotation (euler angles, radians) */
    float angular_vel[3];   /* Angular velocity (rad/s) */
    float scale[3];         /* Current scale (1.0 = original size) */
    float color[4];         /* Current RGBA color */

    float orig_pos[3];      /* Position at decomposition time */
    float orig_color[4];    /* Color at decomposition time */
    float orig_scale[3];    /* Scale at decomposition time */
    float size[3];          /* Box dimensions (half-extents) */

    bool  fallen;           /* True if voxel fell into a pit and is invisible */
    bool  at_rest;          /* True if voxel has come to rest */
    float rest_timer;       /* Accumulates time below speed threshold */

    /* Keyframe trajectory recording for accurate undo reversal */
    VoxelKeyframe keyframes[VOXEL_KEYFRAME_MAX];
    int           keyframe_count;
    float         last_keyframe_time; /* time of last recorded keyframe */
} DeathVoxel;

/* Death animation state */
typedef struct {
    DeathVoxel  voxels[DEATH_VOXEL_MAX];
    int         count;          /* Number of active voxels */

    DeathType   type;
    float       timer;          /* Seconds elapsed (forward or reverse) */
    float       duration;       /* Total forward duration (seconds) */
    bool        active;         /* True while animation is running */
    bool        finished;       /* True when playback (forward or reverse) is done */
    bool        reversing;      /* True during reverse (undo) playback */
    float       reverse_duration; /* Duration of reverse playback */

    /* Actor center (XZ) at decomposition time — for radial scatter */
    float       center_x;
    float       center_z;

    /* Squish-specific: approach direction (normalized XZ) for scatter bias */
    float       approach_dx;
    float       approach_dz;

    /* Environment references for tile queries during simulation */
    const Grid*       grid;
    const BiomeConfig* biome;

    /* Shared unit-cube VBO/VAO (created once, reused for all voxels) */
    GLuint      cube_vao;
    GLuint      cube_vbo;
    int         cube_vertex_count;
} DeathAnim;

/*
 * Initialize a death animation by decomposing the actor mesh.
 *
 * actor_x, actor_z: world-space tile coordinates of the actor (col, row).
 * The actor center is at (actor_x + 0.5, 0, actor_z + 0.5).
 */
void death_anim_init(DeathAnim* da, DeathType type,
                     const ActorParts* actor,
                     float actor_x, float actor_z,
                     const Grid* grid,
                     const BiomeConfig* biome);

/*
 * Set the Minotaur's approach direction for DEATH_SQUISH scatter bias.
 * Call after death_anim_init() and before the first update.
 * dx, dz: direction the Minotaur was moving (need not be normalized).
 */
void death_anim_set_approach(DeathAnim* da, float dx, float dz);

/*
 * Advance the death animation by dt seconds.
 * Applies gravity, floor clamping, pit fall-through, wall collision.
 */
void death_anim_update(DeathAnim* da, float dt);

/*
 * Render all visible death voxels.
 * Caller must have the voxel shader bound with u_vp set.
 */
void death_anim_render(const DeathAnim* da, GLuint shader);

/*
 * Begin reverse playback (undo).
 * Walks each voxel's keyframe array in reverse for frame-accurate reversal.
 */
void death_anim_start_reverse(DeathAnim* da);

/* True when forward or reverse playback is complete. */
bool death_anim_is_finished(const DeathAnim* da);

/* True while the death animation is active (playing or reversing). */
bool death_anim_is_active(const DeathAnim* da);

/* Release GPU resources (shared cube VBO). Safe on zero-initialized struct. */
void death_anim_destroy(DeathAnim* da);

#endif /* DEATH_ANIM_H */
