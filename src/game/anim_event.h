#ifndef GAME_ANIM_EVENT_H
#define GAME_ANIM_EVENT_H

#include "game/feature.h"
#include <stdbool.h>

/*
 * AnimEvent — describes one visual change that needs to animate.
 *
 * Events are recorded during turn_resolve() and replayed by the
 * animation queue. Features push events via grid->active_record.
 *
 * This is a data-driven system: adding a new feature's animation
 * only requires emitting a new event type during resolution.
 */

#define ANIM_EVENT_MAX          32
#define ICE_SLIDE_MAX_WAYPOINTS 16

typedef enum {
    ANIM_EVT_NONE = 0,

    /* ── Theseus phase ─────────────────────────── */
    ANIM_EVT_THESEUS_HOP,          /* normal move: hop A→B */
    ANIM_EVT_THESEUS_ICE_SLIDE,    /* hop to first ice tile, then slide through waypoints */
    ANIM_EVT_THESEUS_TELEPORT,     /* disappear at A, reappear at B */
    ANIM_EVT_MINOTAUR_TELEPORT,    /* minotaur teleport: disappear at A, reappear at B */
    ANIM_EVT_THESEUS_PUSH_MOVE,    /* Theseus steps into vacated tile (groove box) */

    /* Push effects (concurrent with Theseus phase) */
    ANIM_EVT_BOX_SLIDE,            /* groove box slides one tile */
    ANIM_EVT_TURNSTILE_ROTATE,     /* manual turnstile walls rotate 90° */

    /* ── On-leave effects (after Theseus move) ─── */
    ANIM_EVT_FLOOR_CRUMBLE,        /* crumbling floor collapses */
    ANIM_EVT_GATE_LOCK,            /* locking gate bars close */
    ANIM_EVT_PLATE_TOGGLE,         /* pressure plate triggers wall changes */

    /* ── Environment phase ─────────────────────── */
    ANIM_EVT_SPIKE_CHANGE,         /* spike trap extends or retracts */
    ANIM_EVT_AUTO_TURNSTILE_ROTATE,/* auto turnstile rotates walls + actors */
    ANIM_EVT_PLATFORM_MOVE,        /* moving platform slides along path */
    ANIM_EVT_CONVEYOR_PUSH,        /* conveyor pushes actor one tile */
} AnimEventType;

typedef enum {
    ANIM_EVENT_PHASE_THESEUS,       /* during Theseus's move */
    ANIM_EVENT_PHASE_THESEUS_EFFECT,/* on-leave effects after Theseus moves */
    ANIM_EVENT_PHASE_ENVIRONMENT,   /* environment phase */
} AnimEventPhase;

typedef struct {
    AnimEventType  type;
    AnimEventPhase phase;

    /* Common position fields (not all used by every type) */
    int from_col, from_row;
    int to_col,   to_row;
    EntityID entity;                /* which actor, if applicable */

    /* Type-specific data */
    union {
        /* ANIM_EVT_THESEUS_ICE_SLIDE */
        struct {
            int  waypoint_cols[ICE_SLIDE_MAX_WAYPOINTS];
            int  waypoint_rows[ICE_SLIDE_MAX_WAYPOINTS];
            int  waypoint_count;
            bool hit_wall;      /* true if slide ended by hitting a wall */
        } ice_slide;

        /* ANIM_EVT_TURNSTILE_ROTATE / ANIM_EVT_AUTO_TURNSTILE_ROTATE */
        struct {
            int  junction_col, junction_row;
            bool clockwise;
            /* Actor movements caused by rotation */
            int  actor_from_col[2], actor_from_row[2];  /* [0]=theseus, [1]=minotaur */
            int  actor_to_col[2],   actor_to_row[2];
            bool actor_moved[2];
        } turnstile;

        /* ANIM_EVT_PLATFORM_MOVE */
        struct {
            int  platform_from_col, platform_from_row;
            int  platform_to_col,   platform_to_row;
            bool theseus_riding;
            bool minotaur_riding;
        } platform;

        /* ANIM_EVT_CONVEYOR_PUSH */
        struct {
            Direction direction;
        } conveyor;

        /* ANIM_EVT_SPIKE_CHANGE */
        struct {
            bool extended;  /* true = spikes came up, false = retracted */
        } spike;

        /* ANIM_EVT_GATE_LOCK */
        struct {
            Direction gate_side;
        } gate;

        /* ANIM_EVT_PLATE_TOGGLE */
        struct {
            bool toggled;   /* new toggle state */
        } plate;

        /* ANIM_EVT_BOX_SLIDE */
        struct {
            int box_from_col, box_from_row;
            int box_to_col,   box_to_row;
        } box;
    };
} AnimEvent;

#endif /* GAME_ANIM_EVENT_H */
