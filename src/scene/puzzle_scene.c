#include "scene/puzzle_scene.h"
#include "engine/engine.h"
#include "engine/utils.h"
#include "engine/anim_queue.h"
#include "engine/input_buffer.h"
#include "render/renderer.h"
#include "render/ui_draw.h"
#include "render/text_render.h"
#include "render/shader.h"
#include "render/voxel_mesh.h"
#include "render/camera.h"
#include "render/lighting.h"
#include "render/diorama_gen.h"
#include "render/floor_lightmap.h"
#include "render/actor_render.h"
#include "render/dust_puff.h"
#include "data/biome_config.h"
#include "input/input_manager.h"
#include "platform/platform.h"
#include "data/strings.h"
#include "data/settings.h"
#include "game/game.h"
#include "game/features/groove_box.h"
#include "game/features/auto_turnstile.h"
#include "game/features/conveyor.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ---------- Layout constants ---------- */

/* Minimum padding around the grid (pixels) */
#define GRID_PADDING       60.0f
/* Top area reserved for HUD (turns, level name) */
#define HUD_TOP_HEIGHT     50.0f
/* Bottom area reserved for status text */
#define HUD_BOTTOM_HEIGHT  60.0f
/* Wall thickness as fraction of tile size */
#define WALL_THICKNESS_FRAC 0.08f
/* Actor size as fraction of tile size */
#define THESEUS_SIZE_FRAC   0.45f
#define MINOTAUR_SIZE_FRAC  0.65f
/* 3D wall thickness — must match diorama_gen.c WALL_THICKNESS */
#define WALL_THICKNESS_3D   0.20f
/* 3D Theseus body size — must match actor_render.c THESEUS_SIZE */
#define THESEUS_BODY_SIZE   0.45f
/* Exit tile marker inset */
#define EXIT_MARKER_INSET   0.15f
/* Entrance marker inset */
#define ENTRANCE_MARKER_INSET 0.15f

/* ---------- Colors ---------- */

/* Theseus: blue RGB(80, 168, 251) */
static const Color COLOR_THESEUS    = {80.0f/255.0f, 168.0f/255.0f, 251.0f/255.0f, 1.0f};
/* Minotaur: red RGB(239, 34, 34) */
static const Color COLOR_MINOTAUR   = {239.0f/255.0f, 34.0f/255.0f, 34.0f/255.0f, 1.0f};
/* Floor checkerboard */
static const Color COLOR_FLOOR_A    = {0.22f, 0.24f, 0.20f, 1.0f};
static const Color COLOR_FLOOR_B    = {0.26f, 0.28f, 0.24f, 1.0f};
/* Impassable tiles */
static const Color COLOR_IMPASSABLE = {0.12f, 0.13f, 0.11f, 1.0f};
/* Walls */
static const Color COLOR_WALL       = {0.55f, 0.50f, 0.42f, 1.0f};
/* Exit tile glow */
static const Color COLOR_EXIT       = {0.85f, 0.75f, 0.40f, 0.5f};
/* Entrance */
static const Color COLOR_ENTRANCE   = {0.40f, 0.60f, 0.80f, 0.4f};
/* Spike trap inactive */
static const Color COLOR_SPIKE_OFF  = {0.35f, 0.30f, 0.25f, 1.0f};
/* Spike trap active */
static const Color COLOR_SPIKE_ON   = {0.90f, 0.35f, 0.20f, 1.0f};
/* Pressure plate */
static const Color COLOR_PLATE      = {0.30f, 0.50f, 0.70f, 1.0f};
/* Locking gate (unlocked) */
static const Color COLOR_GATE_OPEN  = {0.60f, 0.55f, 0.30f, 1.0f};
/* Locking gate (locked) */
static const Color COLOR_GATE_LOCKED = {0.50f, 0.20f, 0.20f, 1.0f};
/* Teleporter */
static const Color COLOR_TELEPORTER = {0.55f, 0.30f, 0.70f, 1.0f};
/* Crumbling floor (intact) */
static const Color COLOR_CRUMBLE_OK = {0.45f, 0.40f, 0.30f, 1.0f};
/* Crumbling floor (collapsed) */
static const Color COLOR_CRUMBLE_PIT = {0.05f, 0.05f, 0.05f, 1.0f};
/* Moving platform */
static const Color COLOR_PLATFORM   = {0.50f, 0.45f, 0.35f, 1.0f};
/* Medusa wall */
static const Color COLOR_MEDUSA     = {0.40f, 0.65f, 0.30f, 1.0f};
/* Ice tile */
static const Color COLOR_ICE        = {0.55f, 0.75f, 0.85f, 0.6f};
/* Groove box */
static const Color COLOR_GROOVE_BOX = {0.55f, 0.40f, 0.25f, 1.0f};
/* Turnstile */
static const Color COLOR_TURNSTILE  = {0.60f, 0.50f, 0.20f, 1.0f};
/* Conveyor */
static const Color COLOR_CONVEYOR   = {0.45f, 0.55f, 0.35f, 0.8f};
/* Background */
static const Color COLOR_BG         = {0.07f, 0.07f, 0.09f, 1.0f};
/* HUD text */
static const Color COLOR_HUD        = {0.75f, 0.72f, 0.65f, 1.0f};
/* Win text */
static const Color COLOR_WIN        = {0.40f, 0.85f, 0.40f, 1.0f};
/* Loss text */
static const Color COLOR_LOSS       = {0.90f, 0.30f, 0.25f, 1.0f};

/* ---------- Internal state ---------- */

typedef struct {
    State   base;

    /* Level data */
    Grid*      grid;
    UndoStack  undo;
    char       level_path[512];

    /* Computed layout (recalculated on viewport change) */
    float      grid_origin_x;   /* pixel X of col=0, row=0 tile's left edge */
    float      grid_origin_y;   /* pixel Y of col=0, row=0 tile's BOTTOM edge (screen-space top-left origin) */
    float      tile_size;       /* pixels per tile */
    int        last_vw, last_vh;

    /* Status message */
    char       status_text[128];
    Color      status_color;
    float      status_timer;    /* seconds remaining to show status */

    /* Win/loss state for overlay */
    bool       show_result;
    bool       result_is_win;

    /* Animation system */
    AnimQueue   anim;
    InputBuffer input_buf;
    bool        anim_result_pending;  /* true if result overlay should show after anim */
    TurnResult  pending_result;       /* the result to display */

    /* Reverse undo animation: grid restore is deferred until anim completes */
    bool        undo_anim_pending;    /* true during reverse undo animation */

    /* 3D diorama rendering (Step 4 verification) */
    bool            render_3d;       /* 'C' toggles between 2D and 3D view */
    bool            diorama_built;
    VoxelMesh       diorama_mesh;    /* static geometry (floor, walls, exit) */
    ActorParts      theseus_parts;   /* dynamic actor: Theseus */
    ActorParts      minotaur_parts;  /* dynamic actor: Minotaur */
    VoxelMesh       groove_box_mesh; /* dynamic: groove box (wooden crate) */
    GLuint          shadow_vao;
    GLuint          shadow_vbo;      /* simple quad for actor shadow */
    int             shadow_vertex_count;
    GLuint          shadow_tex_theseus;  /* R8 blurred rectangular shadow texture */
    GLuint          shadow_tex_minotaur;
    GLuint          shadow_tex_groovebox;
    float           shadow_extent_t;     /* world-space half-extent of Theseus shadow quad */
    float           shadow_extent_m;     /* world-space half-extent of Minotaur shadow quad */
    float           shadow_extent_gb;    /* world-space half-extent of groove box shadow quad */
    GLuint          shadow_tex_pit;      /* R8 vignette texture for turnstile gear pit shadow */
    float           shadow_extent_pit;   /* world-space half-extent of pit shadow quad */
    float           shadow_offset_x;     /* world-space shadow offset (simulates light angle) */
    float           shadow_offset_z;
    DioramaCamera   diorama_cam;
    LightingState   diorama_light;
    WallStyle       wall_style;      /* cached for shader uniforms at render time */

    /* Auto-turnstile meshes (separated from diorama for rotation animation) */
    #define MAX_AUTO_TURNSTILES 8
    #define TURNSTILE_GEAR_COUNT 12 /* 1 central + up to 11 satellites */
    struct {
        VoxelMesh mesh;           /* walls + raised platform (rotates together) */
        VoxelMesh gears[TURNSTILE_GEAR_COUNT];
        float     gear_cx[TURNSTILE_GEAR_COUNT]; /* gear center X in world space */
        float     gear_cz[TURNSTILE_GEAR_COUNT]; /* gear center Z in world space */
        float     gear_speed[TURNSTILE_GEAR_COUNT]; /* rotation speed multiplier */
        int       gear_count;
        int       jc, jr;         /* junction col/row */
        bool      clockwise;
        bool      valid;
        bool      was_animating;  /* true if this turnstile was animating last frame */
        float     held_angle;     /* rotation to hold after reverse event ends, until undo_pop */
        int       cells[4][2];    /* the 4 tile positions around junction */
    } turnstile_meshes[MAX_AUTO_TURNSTILES];
    int             turnstile_mesh_count;
    BiomeConfig     cached_biome;   /* cached for turnstile mesh regeneration */

    /* Groove trench tracking */
    bool*           groove_tile_map; /* flat bool array [rows * cols], true if tile is on a groove path */
    int             groove_map_cols;
    int             groove_map_rows;
    float           trench_depth;    /* cached from biome config */

    /* Conveyor tile tracking (for actor elevation) */
    bool*           conveyor_tile_map;  /* flat bool array [rows * cols] */
    int             conveyor_map_cols;
    int             conveyor_map_rows;

    /* Turnstile tile tracking (for actor elevation — same height as conveyors) */
    bool*           turnstile_tile_map;  /* flat bool array [rows * cols] */
    int             turnstile_map_cols;
    int             turnstile_map_rows;

    /* Failed push "bump" animation (no game state change, purely visual) */
    bool            bump_active;
    float           bump_timer;      /* 0→1 progress */
    float           bump_dir_x;      /* direction of bump (+1/-1/0) */
    float           bump_dir_z;      /* direction of bump (+1/-1/0) */

    /* Theseus hop deformation (Step 6.3) */
    bool            wobble_active;   /* true while post-hop wobble is playing */
    float           wobble_timer;    /* seconds elapsed since wobble started */
    bool            was_theseus_hopping; /* true if previous frame was in Theseus hop phase */
    float           hop_dir_col;     /* movement direction col (+1/-1/0) for lean */
    float           hop_dir_row;     /* movement direction row (+1/-1/0) for lean */

    /* Minotaur roll deformation (Step 6.4) */
    bool            mino_wobble_active;
    float           mino_wobble_timer;
    bool            was_mino_rolling;  /* true if previous frame was in Minotaur step phase */

    /* Minotaur facing direction (radians, 0 = facing -Z/north) */
    float           mino_facing_angle;

    /* Camera shake (triggered on minotaur stomp) */
    float           shake_timer;       /* seconds remaining (0 = inactive) */
    float           shake_offset_x;    /* current random X offset (world units) */
    float           shake_offset_z;    /* current random Z offset (world units) */

    /* Step1→Step2 transition tracking for mid-roll stomp effects */
    bool            was_mino_in_step1; /* true if previous frame was STEP1 */

    /* Contextual deformations: push, turnstile (Step 6.6) */
    bool            was_pushing;         /* true if previous frame was in BOX_SLIDE phase */
    bool            was_turnstile_pushing; /* true if previous frame was in TURNSTILE_ROTATE */
    float           push_dir_x;          /* direction of last push for recovery */
    float           push_dir_z;
    bool            squish_recovery_active;  /* true while post-push/turnstile/ice elastic recovery plays */
    float           squish_recovery_timer;
    float           squish_recovery_amplitude; /* initial squish amount (sign = overshoot direction) */
    float           squish_recovery_dir_x;
    float           squish_recovery_dir_z;

    /* Debug animation speed (P key cycles through multipliers) */
    float           debug_anim_speed;  /* 0.125, 0.25, 0.5, or 1.0 */
} PuzzleScene;

/* ---------- Layout calculation ---------- */

/*
 * Screen coordinate convention: origin at top-left, Y increases downward.
 * Grid convention: row 0 = south (bottom of board), row max = north (top).
 *
 * We flip the Y so row 0 is rendered at the bottom of the grid area.
 */
static void calc_layout(PuzzleScene* ps, int vw, int vh) {
    ps->last_vw = vw;
    ps->last_vh = vh;

    float avail_w = vw - 2.0f * GRID_PADDING;
    float avail_h = vh - HUD_TOP_HEIGHT - HUD_BOTTOM_HEIGHT - 2.0f * GRID_PADDING;

    if (avail_w <= 0 || avail_h <= 0) {
        ps->tile_size = 1.0f;
        ps->grid_origin_x = 0;
        ps->grid_origin_y = 0;
        return;
    }

    float tile_w = avail_w / ps->grid->cols;
    float tile_h = avail_h / ps->grid->rows;
    ps->tile_size = (tile_w < tile_h) ? tile_w : tile_h;

    /* Cap tile size to something reasonable */
    if (ps->tile_size > 80.0f) ps->tile_size = 80.0f;

    float total_grid_w = ps->tile_size * ps->grid->cols;
    float total_grid_h = ps->tile_size * ps->grid->rows;

    /* Center grid horizontally */
    ps->grid_origin_x = (vw - total_grid_w) * 0.5f;
    /* Place grid vertically: top of grid starts below HUD */
    float grid_top = HUD_TOP_HEIGHT + GRID_PADDING +
                     (avail_h - total_grid_h) * 0.5f;
    /* grid_origin_y = top of grid in screen coords.
     * Row (rows-1) is at the top (north), row 0 at the bottom (south). */
    ps->grid_origin_y = grid_top;
}

/* Convert grid coordinates to screen pixel position (top-left of tile).
 * Float version for smooth animation interpolation. */
static void grid_to_screen_f(const PuzzleScene* ps, float col, float row,
                              float* out_x, float* out_y) {
    *out_x = ps->grid_origin_x + col * ps->tile_size;
    *out_y = ps->grid_origin_y + (ps->grid->rows - 1 - row) * ps->tile_size;
}

/* Integer convenience wrapper */
static void grid_to_screen(const PuzzleScene* ps, int col, int row,
                           float* out_x, float* out_y) {
    grid_to_screen_f(ps, (float)col, (float)row, out_x, out_y);
}

/* ---------- Set status message ---------- */

static void set_status(PuzzleScene* ps, const char* text, Color color, float duration) {
    snprintf(ps->status_text, sizeof(ps->status_text), "%s", text);
    ps->status_color = color;
    ps->status_timer = duration;
}

/* ---------- Handle turn result display ---------- */

static void show_turn_result(PuzzleScene* ps, TurnResult result) {
    switch (result) {
        case TURN_RESULT_WIN:
            ps->show_result = true;
            ps->result_is_win = true;
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "Level Complete! Turns: %d (optimal: %d)",
                         ps->grid->turn_count, ps->grid->optimal_turns);
                set_status(ps, buf, COLOR_WIN, 999.0f);
            }
            break;

        case TURN_RESULT_LOSS_COLLISION:
            ps->show_result = true;
            ps->result_is_win = false;
            set_status(ps, "Caught by the Minotaur! [Z] Undo  [R] Reset",
                       COLOR_LOSS, 999.0f);
            break;

        case TURN_RESULT_LOSS_HAZARD:
            ps->show_result = true;
            ps->result_is_win = false;
            set_status(ps, "Killed by hazard! [Z] Undo  [R] Reset",
                       COLOR_LOSS, 999.0f);
            break;

        case TURN_RESULT_BLOCKED:
        case TURN_RESULT_CONTINUE:
            break;
    }
}

/* ---------- Turn resolution wrapper ---------- */

static Direction action_to_direction(SemanticAction action) {
    switch (action) {
        case ACTION_MOVE_NORTH: return DIR_NORTH;
        case ACTION_MOVE_SOUTH: return DIR_SOUTH;
        case ACTION_MOVE_EAST:  return DIR_EAST;
        case ACTION_MOVE_WEST:  return DIR_WEST;
        default:                return DIR_NONE;
    }
}

/*
 * Resolve an action: handles undo/reset immediately (no animation),
 * or resolves a move/wait through the turn system with animation.
 */
static void resolve_action(PuzzleScene* ps, SemanticAction action) {
    /* Block input during bump animation */
    if (ps->bump_active) return;

    if (ps->show_result) {
        /* In result state, only undo/reset/back allowed */
        if (action == ACTION_UNDO) {
            const TurnRecord* rec = undo_peek_turn_record(&ps->undo);
            if (rec) {
                /* Start reverse animation, defer grid restore */
                ps->show_result = false;
                ps->anim_result_pending = false;
                input_buffer_init(&ps->input_buf);
                ps->undo_anim_pending = true;
                anim_queue_start_reverse(&ps->anim, rec);
                set_status(ps, "Undo", COLOR_HUD, 0.8f);
            } else if (undo_pop(&ps->undo, ps->grid)) {
                /* No TurnRecord — fall back to instant undo */
                ps->show_result = false;
                ps->anim_result_pending = false;
                anim_queue_init(&ps->anim);
                set_status(ps, "Undo", COLOR_HUD, 0.8f);
            }
            return;
        }
        if (action == ACTION_RESET) {
            if (undo_reset(&ps->undo, ps->grid)) {
                ps->show_result = false;
                ps->anim_result_pending = false;
                anim_queue_init(&ps->anim);
                set_status(ps, "Reset", COLOR_HUD, 0.8f);
            }
            return;
        }
        return; /* ignore other actions in result state */
    }

    Direction dir = DIR_NONE;
    bool is_wait = false;

    switch (action) {
        case ACTION_MOVE_NORTH:
        case ACTION_MOVE_SOUTH:
        case ACTION_MOVE_EAST:
        case ACTION_MOVE_WEST:
            dir = action_to_direction(action);
            break;
        case ACTION_WAIT:
            is_wait = true;
            break;
        case ACTION_UNDO: {
            const TurnRecord* rec = undo_peek_turn_record(&ps->undo);
            if (rec) {
                input_buffer_init(&ps->input_buf);
                ps->undo_anim_pending = true;
                anim_queue_start_reverse(&ps->anim, rec);
                set_status(ps, "Undo", COLOR_HUD, 0.8f);
            } else if (undo_pop(&ps->undo, ps->grid)) {
                anim_queue_init(&ps->anim);
                set_status(ps, "Undo", COLOR_HUD, 0.8f);
            }
            return;
        }
        case ACTION_RESET:
            if (undo_reset(&ps->undo, ps->grid)) {
                anim_queue_init(&ps->anim);
                set_status(ps, "Reset", COLOR_HUD, 0.8f);
            }
            return;
        default:
            return;
    }

    /* Push undo snapshot BEFORE resolving */
    undo_push(&ps->undo, ps->grid);

    /* Resolve the turn with animation recording */
    TurnRecord record;
    TurnResult result;
    if (is_wait) {
        result = turn_resolve(ps->grid, DIR_NONE, &record);
    } else {
        result = turn_resolve(ps->grid, dir, &record);
    }

    if (result == TURN_RESULT_BLOCKED) {
        /* Move was blocked, undo the snapshot we just pushed */
        undo_pop(&ps->undo, ps->grid);

        /* Check if there's a groove box at the target tile — if so,
         * play a "bump" animation (Theseus approaches, pushes briefly,
         * then returns to center). No game state change. */
        if (!is_wait && ps->render_3d) {
            int tc = ps->grid->theseus_col + direction_dcol(dir);
            int tr = ps->grid->theseus_row + direction_drow(dir);
            const Cell* target = grid_cell_const(ps->grid, tc, tr);
            bool has_box = false;
            if (target) {
                for (int fi = 0; fi < target->feature_count; fi++) {
                    if (target->features[fi] && target->features[fi]->vt &&
                        target->features[fi]->vt->name &&
                        strcmp(target->features[fi]->vt->name, "groove_box") == 0) {
                        /* Check if the box is actually at this position */
                        if (target->features[fi]->col == tc &&
                            target->features[fi]->row == tr) {
                            has_box = true;
                            break;
                        }
                    }
                }
            }
            if (has_box) {
                ps->bump_active = true;
                ps->bump_timer = 0.0f;
                ps->bump_dir_x = (float)direction_dcol(dir);
                ps->bump_dir_z = (float)direction_drow(dir);
            }
        }
        return;
    }

    /* Store TurnRecord in undo snapshot for reverse animation on undo */
    undo_store_turn_record(&ps->undo, &record);

    /* Start the animation */
    input_buffer_init(&ps->input_buf);
    anim_queue_start(&ps->anim, &record);

    /* Defer result display until animation completes */
    if (result != TURN_RESULT_CONTINUE) {
        ps->anim_result_pending = true;
        ps->pending_result = result;
    } else {
        ps->anim_result_pending = false;
    }
}

/* ---------- Rendering helpers ---------- */

static void render_floor(const PuzzleScene* ps) {
    for (int r = 0; r < ps->grid->rows; r++) {
        for (int c = 0; c < ps->grid->cols; c++) {
            const Cell* cell = grid_cell_const(ps->grid, c, r);
            float sx, sy;
            grid_to_screen(ps, c, r, &sx, &sy);

            Color col;
            if (cell->impassable) {
                col = COLOR_IMPASSABLE;
            } else {
                col = ((c + r) % 2 == 0) ? COLOR_FLOOR_A : COLOR_FLOOR_B;
            }

            ui_draw_rect(sx, sy, ps->tile_size, ps->tile_size, col);
        }
    }
}

static void render_features(const PuzzleScene* ps) {
    float ts = ps->tile_size;
    float inset = ts * 0.15f;

    for (int i = 0; i < ps->grid->feature_count; i++) {
        const Feature* f = ps->grid->features[i];
        const char* name = f->vt->name;
        float sx, sy;
        grid_to_screen(ps, f->col, f->row, &sx, &sy);

        if (strcmp(name, "spike_trap") == 0) {
            bool active = f->vt->is_hazardous &&
                          f->vt->is_hazardous(f, ps->grid, f->col, f->row);
            Color spike_color = active ? COLOR_SPIKE_ON : COLOR_SPIKE_OFF;
            ui_draw_rect(sx + inset, sy + inset,
                         ts - 2.0f * inset, ts - 2.0f * inset, spike_color);
            if (active) {
                float dot = ts * 0.08f;
                ui_draw_rect(sx + inset, sy + inset, dot, dot, COLOR_SPIKE_ON);
                ui_draw_rect(sx + ts - inset - dot, sy + inset, dot, dot, COLOR_SPIKE_ON);
                ui_draw_rect(sx + inset, sy + ts - inset - dot, dot, dot, COLOR_SPIKE_ON);
                ui_draw_rect(sx + ts - inset - dot, sy + ts - inset - dot, dot, dot, COLOR_SPIKE_ON);
            }
        } else if (strcmp(name, "pressure_plate") == 0) {
            /* Recessed plate marker */
            float pi = ts * 0.25f;
            ui_draw_rect(sx + pi, sy + pi, ts - 2.0f * pi, ts - 2.0f * pi, COLOR_PLATE);
        } else if (strcmp(name, "locking_gate") == 0) {
            /* Small gate marker on the tile */
            typedef struct { int gate_side; bool locked; } LGPeek;
            LGPeek* lgd = (LGPeek*)f->data;
            Color gc = lgd->locked ? COLOR_GATE_LOCKED : COLOR_GATE_OPEN;
            float gi = ts * 0.30f;
            ui_draw_rect(sx + gi, sy + gi, ts - 2.0f * gi, ts - 2.0f * gi, gc);
        } else if (strcmp(name, "teleporter") == 0) {
            float ti = ts * 0.25f;
            ui_draw_rect(sx + ti, sy + ti, ts - 2.0f * ti, ts - 2.0f * ti, COLOR_TELEPORTER);
        } else if (strcmp(name, "crumbling_floor") == 0) {
            bool collapsed = f->vt->is_hazardous &&
                             f->vt->is_hazardous(f, ps->grid, f->col, f->row);
            Color cc = collapsed ? COLOR_CRUMBLE_PIT : COLOR_CRUMBLE_OK;
            float ci = ts * 0.10f;
            ui_draw_rect(sx + ci, sy + ci, ts - 2.0f * ci, ts - 2.0f * ci, cc);
            if (!collapsed) {
                float line_w = 2.0f;
                ui_draw_rect(sx + ts * 0.3f, sy + ts * 0.2f, line_w, ts * 0.6f, COLOR_BG);
                ui_draw_rect(sx + ts * 0.2f, sy + ts * 0.5f, ts * 0.5f, line_w, COLOR_BG);
            }
        } else if (strcmp(name, "moving_platform") == 0) {
            float pi = ts * 0.08f;
            ui_draw_rect(sx + pi, sy + pi, ts - 2.0f * pi, ts - 2.0f * pi, COLOR_PLATFORM);
        } else if (strcmp(name, "medusa_wall") == 0) {
            float ei = ts * 0.30f;
            ui_draw_rect(sx + ei, sy + ei, ts - 2.0f * ei, ts - 2.0f * ei, COLOR_MEDUSA);
        } else if (strcmp(name, "ice_tile") == 0) {
            ui_draw_rect(sx, sy, ts, ts, COLOR_ICE);
        } else if (strcmp(name, "groove_box") == 0) {
            float bi = ts * 0.12f;
            ui_draw_rect(sx + bi, sy + bi, ts - 2.0f * bi, ts - 2.0f * bi,
                         COLOR_GROOVE_BOX);
        } else if (strcmp(name, "auto_turnstile") == 0 ||
                   strcmp(name, "manual_turnstile") == 0) {
            float gi = ts * 0.35f;
            ui_draw_rect(sx + gi, sy + gi, ts - 2.0f * gi, ts - 2.0f * gi,
                         COLOR_TURNSTILE);
        } else if (strcmp(name, "conveyor") == 0) {
            float ci = ts * 0.05f;
            ui_draw_rect(sx + ci, sy + ci, ts - 2.0f * ci, ts - 2.0f * ci,
                         COLOR_CONVEYOR);

            typedef struct { Direction dir; } ConvPeek;
            ConvPeek* cd = (ConvPeek*)f->data;
            float cx = sx + ts * 0.5f;
            float cy = sy + ts * 0.5f;
            float aw = ts * 0.08f;
            float al = ts * 0.25f;
            Color ac = {0.25f, 0.35f, 0.20f, 1.0f};

            if (cd->dir == DIR_EAST || cd->dir == DIR_WEST) {
                ui_draw_rect(cx - al, cy - aw * 0.5f, al * 2.0f, aw, ac);
            } else {
                ui_draw_rect(cx - aw * 0.5f, cy - al, aw, al * 2.0f, ac);
            }

            float ah = ts * 0.12f;
            float hx = cx, hy = cy;
            if (cd->dir == DIR_EAST)  hx = cx + al;
            if (cd->dir == DIR_WEST)  hx = cx - al;
            if (cd->dir == DIR_NORTH) hy = cy - al;
            if (cd->dir == DIR_SOUTH) hy = cy + al;
            ui_draw_rect(hx - ah * 0.5f, hy - ah * 0.5f, ah, ah, ac);
        }
    }
}

static void render_doors(const PuzzleScene* ps) {
    float ts = ps->tile_size;

    /* Exit tile marker */
    {
        float sx, sy;
        grid_to_screen(ps, ps->grid->exit_col, ps->grid->exit_row, &sx, &sy);
        float inset_val = ts * EXIT_MARKER_INSET;
        ui_draw_rect(sx + inset_val, sy + inset_val,
                     ts - 2.0f * inset_val, ts - 2.0f * inset_val,
                     COLOR_EXIT);
    }

    /* Entrance tile marker */
    {
        float sx, sy;
        grid_to_screen(ps, ps->grid->entrance_col, ps->grid->entrance_row, &sx, &sy);
        float inset_val = ts * ENTRANCE_MARKER_INSET;
        ui_draw_rect(sx + inset_val, sy + inset_val,
                     ts - 2.0f * inset_val, ts - 2.0f * inset_val,
                     COLOR_ENTRANCE);
    }
}

static void render_walls(const PuzzleScene* ps) {
    float ts = ps->tile_size;
    float wt = ts * WALL_THICKNESS_FRAC;
    if (wt < 2.0f) wt = 2.0f;  /* minimum visible wall */

    for (int r = 0; r < ps->grid->rows; r++) {
        for (int c = 0; c < ps->grid->cols; c++) {
            float sx, sy;
            grid_to_screen(ps, c, r, &sx, &sy);

            /* North wall (top edge of this tile in screen coords) */
            if (grid_has_wall(ps->grid, c, r, DIR_NORTH)) {
                if (!(c == ps->grid->exit_col && r == ps->grid->exit_row &&
                      ps->grid->exit_side == DIR_NORTH)) {
                    ui_draw_rect(sx - wt * 0.5f, sy - wt * 0.5f,
                                 ts + wt, wt, COLOR_WALL);
                }
            }

            /* South wall */
            if (grid_has_wall(ps->grid, c, r, DIR_SOUTH)) {
                if (!(c == ps->grid->exit_col && r == ps->grid->exit_row &&
                      ps->grid->exit_side == DIR_SOUTH)) {
                    ui_draw_rect(sx - wt * 0.5f, sy + ts - wt * 0.5f,
                                 ts + wt, wt, COLOR_WALL);
                }
            }

            /* West wall */
            if (grid_has_wall(ps->grid, c, r, DIR_WEST)) {
                if (!(c == ps->grid->exit_col && r == ps->grid->exit_row &&
                      ps->grid->exit_side == DIR_WEST)) {
                    ui_draw_rect(sx - wt * 0.5f, sy - wt * 0.5f,
                                 wt, ts + wt, COLOR_WALL);
                }
            }

            /* East wall */
            if (grid_has_wall(ps->grid, c, r, DIR_EAST)) {
                if (!(c == ps->grid->exit_col && r == ps->grid->exit_row &&
                      ps->grid->exit_side == DIR_EAST)) {
                    ui_draw_rect(sx + ts - wt * 0.5f, sy - wt * 0.5f,
                                 wt, ts + wt, COLOR_WALL);
                }
            }
        }
    }
}

static void render_actors(const PuzzleScene* ps) {
    float ts = ps->tile_size;
    bool animating = anim_queue_is_playing(&ps->anim);

    /* Minotaur (drawn first so Theseus appears on top) */
    {
        float msize = ts * MINOTAUR_SIZE_FRAC;
        float sx, sy;

        if (animating) {
            float mcol, mrow;
            anim_queue_minotaur_pos(&ps->anim, &mcol, &mrow);
            grid_to_screen_f(ps, mcol, mrow, &sx, &sy);
        } else {
            grid_to_screen(ps, ps->grid->minotaur_col, ps->grid->minotaur_row, &sx, &sy);
        }

        float offset = (ts - msize) * 0.5f;
        ui_draw_rect(sx + offset, sy + offset, msize, msize, COLOR_MINOTAUR);

        /* Small horn indicators (white dots at top corners) */
        float horn_sz = msize * 0.15f;
        ui_draw_rect(sx + offset, sy + offset - horn_sz,
                     horn_sz, horn_sz, COLOR_WHITE);
        ui_draw_rect(sx + offset + msize - horn_sz, sy + offset - horn_sz,
                     horn_sz, horn_sz, COLOR_WHITE);
    }

    /* Theseus */
    {
        float tsize = ts * THESEUS_SIZE_FRAC;
        float sx, sy;

        if (animating) {
            float tcol, trow, thop;
            anim_queue_theseus_pos(&ps->anim, &tcol, &trow, &thop);
            grid_to_screen_f(ps, tcol, trow, &sx, &sy);
            /* Apply hop offset (negative Y = upward in screen space) */
            sy -= thop * ts;

            /* Teleport effect: scale Theseus based on progress */
            int tp_phase;
            float tp_prog = anim_queue_teleport_progress(&ps->anim, &tp_phase);
            if (tp_prog >= 0.0f) {
                float scale;
                if (tp_phase == 0) {
                    /* Fading out: scale 1→0 */
                    scale = 1.0f - tp_prog;
                } else {
                    /* Fading in: scale 0→1 */
                    scale = tp_prog;
                }
                float scaled_size = tsize * scale;
                float offset = (ts - scaled_size) * 0.5f;
                Color tc = COLOR_THESEUS;
                tc.a = scale;
                ui_draw_rect(sx + offset, sy + offset, scaled_size, scaled_size, tc);
            } else {
                float offset = (ts - tsize) * 0.5f;
                ui_draw_rect(sx + offset, sy + offset, tsize, tsize, COLOR_THESEUS);
            }
        } else {
            grid_to_screen(ps, ps->grid->theseus_col, ps->grid->theseus_row, &sx, &sy);
            float offset = (ts - tsize) * 0.5f;
            ui_draw_rect(sx + offset, sy + offset, tsize, tsize, COLOR_THESEUS);
        }
    }

    /* Groove box during push animation */
    if (animating && anim_queue_theseus_event_type(&ps->anim) == ANIM_EVT_BOX_SLIDE &&
        anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS) {
        float bcol, brow;
        anim_queue_aux_pos(&ps->anim, &bcol, &brow);
        float bsx, bsy;
        grid_to_screen_f(ps, bcol, brow, &bsx, &bsy);
        float bi = ts * 0.12f;
        ui_draw_rect(bsx + bi, bsy + bi, ts - 2.0f * bi, ts - 2.0f * bi,
                     COLOR_GROOVE_BOX);
    }
}

static void render_hud(const PuzzleScene* ps, int vw, int vh) {
    /* Turn counter — top left */
    char turn_buf[64];
    snprintf(turn_buf, sizeof(turn_buf), "Turn: %d", ps->grid->turn_count);
    text_render_draw(turn_buf, 20.0f, 15.0f, TEXT_SIZE_BODY, COLOR_HUD,
                     TEXT_ALIGN_LEFT);

    /* Level name — top center */
    text_render_draw(ps->grid->level_name, vw * 0.5f, 15.0f, TEXT_SIZE_BODY,
                     COLOR_HUD, TEXT_ALIGN_CENTER);

    /* Optimal turns — top right */
    char opt_buf[64];
    snprintf(opt_buf, sizeof(opt_buf), "Optimal: %d", ps->grid->optimal_turns);
    text_render_draw(opt_buf, vw - 20.0f, 15.0f, TEXT_SIZE_BODY, COLOR_HUD,
                     TEXT_ALIGN_RIGHT);

    /* Undo depth — below turn counter */
    int depth = undo_depth(&ps->undo);
    if (depth > 0) {
        char undo_buf[64];
        snprintf(undo_buf, sizeof(undo_buf), "Undo: %d", depth);
        text_render_draw(undo_buf, 20.0f, 42.0f, TEXT_SIZE_SMALL,
                         color_rgba(0.5f, 0.5f, 0.5f, 1.0f), TEXT_ALIGN_LEFT);
    }

    /* Status text — bottom center */
    if (ps->status_timer > 0.0f) {
        text_render_draw(ps->status_text, vw * 0.5f,
                         vh - HUD_BOTTOM_HEIGHT + 15.0f,
                         TEXT_SIZE_BODY, ps->status_color, TEXT_ALIGN_CENTER);
    }

    /* Controls hint — bottom */
    if (!ps->show_result) {
        text_render_draw("[Arrows] Move  [Space] Wait  [Z] Undo  [R] Reset  [Esc] Back",
                         vw * 0.5f, vh - 20.0f, TEXT_SIZE_SMALL,
                         color_rgba(0.4f, 0.4f, 0.4f, 1.0f), TEXT_ALIGN_CENTER);
    }
}

static void render_debug_labels(const PuzzleScene* ps, int vw, int vh) {
    Color label_color = color_rgba(0.5f, 0.5f, 0.5f, 0.8f);
    float x = vw - 15.0f;
    float y = 60.0f;
    float line_h = 18.0f;

    /* Render mode */
    text_render_draw(ps->render_3d ? "[C] 3D" : "[C] 2D",
                     x, y, TEXT_SIZE_SMALL, label_color, TEXT_ALIGN_RIGHT);
    y += line_h;

    /* Projection mode */
    text_render_draw(g_settings.camera_perspective ? "[V] Perspective" : "[V] Orthographic",
                     x, y, TEXT_SIZE_SMALL, label_color, TEXT_ALIGN_RIGHT);
    y += line_h;

    /* Cel-shading */
    text_render_draw(g_settings.cel_shading ? "[B] Cel-Shaded" : "[B] Standard",
                     x, y, TEXT_SIZE_SMALL, label_color, TEXT_ALIGN_RIGHT);
    y += line_h;

    /* Camera pitch */
    char pitch_buf[64];
    snprintf(pitch_buf, sizeof(pitch_buf), "[I/K] Pitch: %.0f\xC2\xB0",
             ps->diorama_cam.pitch);
    text_render_draw(pitch_buf, x, y, TEXT_SIZE_SMALL, label_color, TEXT_ALIGN_RIGHT);
    y += line_h;

    /* FOV */
    char fov_buf[64];
    snprintf(fov_buf, sizeof(fov_buf), "[O/L] FOV: %.0f\xC2\xB0",
             g_settings.camera_fov);
    text_render_draw(fov_buf, x, y, TEXT_SIZE_SMALL, label_color, TEXT_ALIGN_RIGHT);
    y += line_h;

    /* Animation speed (only show when not 1.0) */
    if (ps->debug_anim_speed < 0.99f) {
        char speed_buf[64];
        snprintf(speed_buf, sizeof(speed_buf), "[P] Speed: %.3fx",
                 ps->debug_anim_speed);
        Color speed_color = color_rgba(1.0f, 0.8f, 0.2f, 0.9f);
        text_render_draw(speed_buf, x, y, TEXT_SIZE_SMALL, speed_color, TEXT_ALIGN_RIGHT);
    }
}

static void render_result_overlay(const PuzzleScene* ps, int vw, int vh) {
    if (!ps->show_result) return;

    /* Semi-transparent overlay */
    ui_draw_rect(0, 0, (float)vw, (float)vh, color_rgba(0.0f, 0.0f, 0.0f, 0.5f));

    float cy = vh * 0.45f;

    if (ps->result_is_win) {
        text_render_draw("Level Complete!",
                         vw * 0.5f, cy - 40.0f, TEXT_SIZE_TITLE,
                         COLOR_WIN, TEXT_ALIGN_CENTER);

        char turn_text[128];
        snprintf(turn_text, sizeof(turn_text), "Turns: %d  (Optimal: %d)",
                 ps->grid->turn_count, ps->grid->optimal_turns);
        text_render_draw(turn_text, vw * 0.5f, cy + 20.0f, TEXT_SIZE_LARGE,
                         COLOR_HUD, TEXT_ALIGN_CENTER);

        /* Star rating */
        const char* stars;
        if (ps->grid->turn_count <= ps->grid->optimal_turns) {
            stars = "\xE2\x98\x85\xE2\x98\x85";   /* ★★ */
        } else {
            stars = "\xE2\x98\x85";                 /* ★ */
        }
        text_render_draw(stars, vw * 0.5f, cy + 60.0f, TEXT_SIZE_TITLE,
                         color_rgba(1.0f, 0.85f, 0.2f, 1.0f), TEXT_ALIGN_CENTER);

        text_render_draw("[Esc] Back  [R] Retry",
                         vw * 0.5f, cy + 120.0f, TEXT_SIZE_BODY,
                         COLOR_HUD, TEXT_ALIGN_CENTER);
    } else {
        text_render_draw("Defeat",
                         vw * 0.5f, cy - 20.0f, TEXT_SIZE_TITLE,
                         COLOR_LOSS, TEXT_ALIGN_CENTER);

        text_render_draw("[Z] Undo  [R] Reset  [Esc] Back",
                         vw * 0.5f, cy + 40.0f, TEXT_SIZE_BODY,
                         COLOR_HUD, TEXT_ALIGN_CENTER);
    }
}

/* ---------- 3D Diorama (Step 4 verification) ---------- */

/*
 * Build a hardcoded test diorama from the current grid:
 *   - Checkerboard floor tiles (slight height variation)
 *   - Wall blocks
 *   - Theseus and Minotaur as colored cubes
 * All in world units where 1 unit = 1 tile.
 */

/* Generate a precomputed soft radial shadow texture and a flat quad to render it.
 * The texture is a 64×64 R8 image with a smooth gaussian-like falloff from center.
 * The quad uses the same vertex layout as voxel meshes (pos + normal + color + uv). */
#define SHADOW_TEX_SIZE     64   /* texels per side for actor shadow texture */
#define SHADOW_FLOATS_PER_VERT 13  /* pos(3)+norm(3)+col(4)+uv(2)+ao_mode(1) */

/* ---------- Gaussian blur (same algorithm as floor_lightmap) ---------- */

static void shadow_gaussian_blur(float* data, int w, int h, float radius) {
    if (radius < 0.5f) return;
    int half_k = (int)ceilf(radius * 2.0f);
    if (half_k < 1) half_k = 1;
    if (half_k > 32) half_k = 32;
    int ks = half_k * 2 + 1;
    float* kernel = (float*)malloc((size_t)ks * sizeof(float));
    float sigma = radius, sum = 0.0f;
    for (int i = 0; i < ks; i++) {
        float x = (float)(i - half_k);
        kernel[i] = expf(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < ks; i++) kernel[i] /= sum;
    float* temp = (float*)malloc((size_t)w * (size_t)h * sizeof(float));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float v = 0.0f;
            for (int k = -half_k; k <= half_k; k++) {
                int sx = x + k; if (sx < 0) sx = 0; if (sx >= w) sx = w - 1;
                v += data[y * w + sx] * kernel[k + half_k];
            }
            temp[y * w + x] = v;
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float v = 0.0f;
            for (int k = -half_k; k <= half_k; k++) {
                int sy = y + k; if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
                v += temp[sy * w + x] * kernel[k + half_k];
            }
            data[y * w + x] = v;
        }
    free(temp);
    free(kernel);
}

/* Check if a tile position is on a groove path.
 * Works for integer tile coordinates; for fractional positions during
 * animation, caller should round or check both from/to tiles. */
static bool is_groove_tile(const PuzzleScene* ps, int col, int row) {
    if (!ps->groove_tile_map) return false;
    if (col < 0 || col >= ps->groove_map_cols) return false;
    if (row < 0 || row >= ps->groove_map_rows) return false;
    return ps->groove_tile_map[row * ps->groove_map_cols + col];
}

static bool is_conveyor_tile(const PuzzleScene* ps, int col, int row) {
    if (!ps->conveyor_tile_map) return false;
    if (col < 0 || col >= ps->conveyor_map_cols) return false;
    if (row < 0 || row >= ps->conveyor_map_rows) return false;
    return ps->conveyor_tile_map[row * ps->conveyor_map_cols + col];
}

static bool is_turnstile_tile(const PuzzleScene* ps, int col, int row) {
    if (!ps->turnstile_tile_map) return false;
    if (col < 0 || col >= ps->turnstile_map_cols) return false;
    if (row < 0 || row >= ps->turnstile_map_rows) return false;
    return ps->turnstile_tile_map[row * ps->turnstile_map_cols + col];
}

/* Check if a tile is any elevated platform (conveyor or turnstile) */
static bool is_elevated_tile(const PuzzleScene* ps, int col, int row) {
    return is_conveyor_tile(ps, col, row) || is_turnstile_tile(ps, col, row);
}

/*
 * Generate a blurred rectangular shadow texture for an actor.
 *
 * footprint: world-space footprint size (square actor)
 * cfg:       floor shadow config (softness, blur_radius, intensity, scale)
 * out_tex:   receives the GL texture handle
 * out_extent: receives the world-space half-extent of the shadow quad
 */
static void generate_actor_shadow_texture(float footprint, float actor_height,
                                           const ActorShadowConfig* cfg,
                                           GLuint* out_tex, float* out_extent) {
    int tex_size = SHADOW_TEX_SIZE;

    /* Convert floor lightmap blur parameters to world-space sigma.
     * The floor lightmap uses blur_radius in texels at shadow_resolution
     * texels per tile (1 tile = 1.0 world units). */
    float resolution = (float)cfg->shadow_resolution;
    if (resolution < 4.0f) resolution = 4.0f;
    float world_sigma = cfg->shadow_softness * cfg->shadow_blur_radius / resolution;

    /* Shadow scale: taller actors cast slightly wider shadows (simulates
     * a non-point overhead light source). Base scale from config, plus
     * a height-proportional expansion. */
    float height_factor = 1.0f + actor_height * 0.5f;
    float scaled_foot = footprint * cfg->shadow_scale * height_factor;

    /* Shadow extent: half-footprint + 4*sigma padding for full blur decay */
    float world_extent = scaled_foot * 0.5f + world_sigma * 4.0f;
    if (world_extent < scaled_foot * 0.6f) world_extent = scaled_foot * 0.6f;

    /* Rasterize footprint into float buffer */
    float* shadow = (float*)calloc((size_t)tex_size * (size_t)tex_size, sizeof(float));
    float half_foot = scaled_foot * 0.5f;

    for (int ty = 0; ty < tex_size; ty++) {
        for (int tx = 0; tx < tex_size; tx++) {
            /* Map texel to world-space offset from center */
            float wx = ((float)tx + 0.5f) / (float)tex_size * 2.0f * world_extent - world_extent;
            float wz = ((float)ty + 0.5f) / (float)tex_size * 2.0f * world_extent - world_extent;

            /* Inside the scaled footprint? */
            if (fabsf(wx) <= half_foot && fabsf(wz) <= half_foot) {
                shadow[ty * tex_size + tx] = cfg->shadow_intensity;
            }
        }
    }

    /* Convert world-space sigma to texel-space blur for this texture.
     * texels_per_unit = tex_size / (2 * world_extent). */
    float texels_per_unit = (float)tex_size / (2.0f * world_extent);
    float actual_blur = world_sigma * texels_per_unit;
    shadow_gaussian_blur(shadow, tex_size, tex_size, actual_blur);

    /* Convert to "lit" space: 1=lit, 0=dark (matches floor lightmap convention) */
    for (int i = 0; i < tex_size * tex_size; i++) {
        shadow[i] = 1.0f - shadow[i];
    }

    /* Convert to uint8 and upload as R8 texture */
    uint8_t* texels = (uint8_t*)malloc((size_t)tex_size * (size_t)tex_size);
    for (int i = 0; i < tex_size * tex_size; i++) {
        float v = shadow[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        texels[i] = (uint8_t)(v * 255.0f + 0.5f);
    }
    free(shadow);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, tex_size, tex_size, 0,
                 GL_RED, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(texels);

    *out_tex = tex;
    *out_extent = world_extent;
}

static void build_shadow_resources(PuzzleScene* ps, const ActorShadowConfig* cfg) {
    /* Generate blurred rectangular shadow textures for each actor.
     * Uses the same blur/softness/intensity parameters as wall floor shadows. */
    /* Theseus: cube (size × size × size), so height = size */
    generate_actor_shadow_texture(THESEUS_SIZE_FRAC, THESEUS_SIZE_FRAC, cfg,
                                   &ps->shadow_tex_theseus, &ps->shadow_extent_t);
    /* Minotaur: true cube (size × size × size) */
    generate_actor_shadow_texture(MINOTAUR_SIZE_FRAC, MINOTAUR_SIZE_FRAC, cfg,
                                   &ps->shadow_tex_minotaur, &ps->shadow_extent_m);
    /* Groove box: footprint = tile minus inset, height matches box_h */
    float gb_footprint = 1.0f - 2.0f * 0.12f;  /* matches groove box inset */
    generate_actor_shadow_texture(gb_footprint, 0.45f, cfg,
                                   &ps->shadow_tex_groovebox, &ps->shadow_extent_gb);

    /* Turnstile gear pit vignette shadow: dark around the 2×2 perimeter,
     * transparent in the center so gears remain visible through the shadow.
     * The pit covers a 2×2 tile area (extent = 1.0 from center). */
    {
        int tex_size = SHADOW_TEX_SIZE;
        float extent = 1.05f;  /* slight oversize to cover floor edges */
        float* shadow = (float*)calloc((size_t)tex_size * (size_t)tex_size, sizeof(float));
        float inset = 0.25f;   /* shadow width in world units from edge inward */
        float intensity = 0.55f;

        for (int ty = 0; ty < tex_size; ty++) {
            for (int tx = 0; tx < tex_size; tx++) {
                /* Map texel to [-extent, +extent] world space */
                float wx = ((float)tx + 0.5f) / (float)tex_size * 2.0f * extent - extent;
                float wz = ((float)ty + 0.5f) / (float)tex_size * 2.0f * extent - extent;

                /* Distance from nearest edge of the 1.0-radius square */
                float dx = 1.0f - fabsf(wx);  /* distance from left/right edge */
                float dz = 1.0f - fabsf(wz);  /* distance from top/bottom edge */
                float d = fminf(dx, dz);       /* distance from nearest edge */

                /* Outside the pit area: no shadow */
                if (d < 0.0f) continue;

                /* Smooth gradient from edge (d=0) to inset depth */
                if (d < inset) {
                    float t = d / inset;               /* 0 at edge → 1 at inset */
                    float smooth = t * t * (3.0f - 2.0f * t);  /* smoothstep */
                    shadow[ty * tex_size + tx] = intensity * (1.0f - smooth);
                }
                /* d >= inset: shadow stays 0 (fully lit center) */
            }
        }

        /* Light Gaussian blur for smoother edges */
        shadow_gaussian_blur(shadow, tex_size, tex_size, 2.0f);

        /* Convert to "lit" space: 255=lit, 0=dark */
        uint8_t* texels = (uint8_t*)malloc((size_t)tex_size * (size_t)tex_size);
        for (int i = 0; i < tex_size * tex_size; i++) {
            float v = 1.0f - shadow[i];
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            texels[i] = (uint8_t)(v * 255.0f + 0.5f);
        }
        free(shadow);

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, tex_size, tex_size, 0,
                     GL_RED, GL_UNSIGNED_BYTE, texels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        free(texels);

        ps->shadow_tex_pit = tex;
        ps->shadow_extent_pit = extent;
    }

    /* Cache shadow offsets for draw-time positioning (same as wall shadows) */
    ps->shadow_offset_x = cfg->shadow_offset_x;
    ps->shadow_offset_z = cfg->shadow_offset_z;

    /* Build a simple quad (2 triangles, 6 vertices).
     * Positions span [-1, +1] in X and Z — scaled by model matrix.
     * UVs span [0, 1] for shadow texture sampling.
     * ao_mode = AO_MODE_SHADOW (3.0) so the shader samples the texture. */
    float verts[6 * SHADOW_FLOATS_PER_VERT];
    int vi = 0;

    #define SHADOW_VERT(px, pz, u, v) do { \
        float* dst = &verts[vi * SHADOW_FLOATS_PER_VERT]; \
        dst[0] = (px); dst[1] = 0.0f; dst[2] = (pz); \
        dst[3] = 0.0f; dst[4] = 1.0f; dst[5] = 0.0f; \
        dst[6] = 0.0f; dst[7] = 0.0f; dst[8] = 0.0f; dst[9] = 1.0f; \
        dst[10] = (u); dst[11] = (v); \
        dst[12] = 3.0f; /* AO_MODE_SHADOW */ \
        vi++; \
    } while (0)

    /* Two triangles: (-1,-1) to (+1,+1) */
    SHADOW_VERT(-1.0f, -1.0f, 0.0f, 0.0f);
    SHADOW_VERT( 1.0f, -1.0f, 1.0f, 0.0f);
    SHADOW_VERT( 1.0f,  1.0f, 1.0f, 1.0f);
    SHADOW_VERT(-1.0f, -1.0f, 0.0f, 0.0f);
    SHADOW_VERT( 1.0f,  1.0f, 1.0f, 1.0f);
    SHADOW_VERT(-1.0f,  1.0f, 0.0f, 1.0f);

    #undef SHADOW_VERT

    ps->shadow_vertex_count = vi;

    glGenVertexArrays(1, &ps->shadow_vao);
    glGenBuffers(1, &ps->shadow_vbo);
    glBindVertexArray(ps->shadow_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ps->shadow_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(sizeof(verts)),
                 verts, GL_STATIC_DRAW);

    size_t stride = SHADOW_FLOATS_PER_VERT * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(12 * sizeof(float)));

    glBindVertexArray(0);
}

static void destroy_shadow_resources(PuzzleScene* ps) {
    if (ps->shadow_vao) { glDeleteVertexArrays(1, &ps->shadow_vao); ps->shadow_vao = 0; }
    if (ps->shadow_vbo) { glDeleteBuffers(1, &ps->shadow_vbo); ps->shadow_vbo = 0; }
    if (ps->shadow_tex_theseus) { glDeleteTextures(1, &ps->shadow_tex_theseus); ps->shadow_tex_theseus = 0; }
    if (ps->shadow_tex_minotaur) { glDeleteTextures(1, &ps->shadow_tex_minotaur); ps->shadow_tex_minotaur = 0; }
    if (ps->shadow_tex_groovebox) { glDeleteTextures(1, &ps->shadow_tex_groovebox); ps->shadow_tex_groovebox = 0; }
    if (ps->shadow_tex_pit) { glDeleteTextures(1, &ps->shadow_tex_pit); ps->shadow_tex_pit = 0; }
    ps->shadow_vertex_count = 0;
}

/* ---------- Matrix helpers for roll animation ---------- */

/* Build a 4×4 column-major identity matrix. */
static void mat4_identity(float m[16]) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* Build a 4×4 column-major translation matrix. */
static void mat4_translate(float m[16], float tx, float ty, float tz) {
    mat4_identity(m);
    m[12] = tx; m[13] = ty; m[14] = tz;
}

/* Multiply two 4×4 column-major matrices: out = a * b.
 * out may NOT alias a or b. */
static void mat4_mul(float out[16], const float a[16], const float b[16]) {
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            out[c * 4 + r] =
                a[0 * 4 + r] * b[c * 4 + 0] +
                a[1 * 4 + r] * b[c * 4 + 1] +
                a[2 * 4 + r] * b[c * 4 + 2] +
                a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
}

/* Build a rotation matrix around the X axis (column-major). */
static void mat4_rot_x(float m[16], float angle_rad) {
    mat4_identity(m);
    float c = cosf(angle_rad), s = sinf(angle_rad);
    m[5] = c;  m[6] = s;
    m[9] = -s; m[10] = c;
}

/* Build a rotation matrix around the Z axis (column-major). */
static void mat4_rot_z(float m[16], float angle_rad) {
    mat4_identity(m);
    float c = cosf(angle_rad), s = sinf(angle_rad);
    m[0] = c;  m[1] = s;
    m[4] = -s; m[5] = c;
}

/* Build a rotation matrix around the Y axis (column-major). */
static void mat4_rot_y(float m[16], float angle_rad) {
    mat4_identity(m);
    float c = cosf(angle_rad), s = sinf(angle_rad);
    m[0] = c;  m[2] = -s;
    m[8] = s;  m[10] = c;
}

/* Build a Y-axis scale matrix (for horn retraction). */
static void mat4_scale_y(float m[16], float sy) {
    mat4_identity(m);
    m[5] = sy;
}

static void draw_shadow_at_y_rot(const PuzzleScene* ps, GLuint shader,
                              float x, float y, float z, float scale,
                              float rot_y,
                              GLuint shadow_tex, float extent) {
    if (ps->shadow_vertex_count == 0 || !shadow_tex) return;

    /* Bind actor shadow texture to unit 1 (temporarily replaces floor lightmap) */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadow_tex);
    glActiveTexture(GL_TEXTURE0);

    /* Disable writes to the normal buffer (attachment 1) during shadow
     * rendering.  Shadow blending would otherwise corrupt the NormalOut
     * alpha channel that encodes material boundaries (conveyor vs floor)
     * used by the outline post-process shader. */
    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    /* Disable depth writes so shadow doesn't block actor rendering.
     * Caller sets depth func (GL_LESS for normal, GL_GREATER for rim).
     * Polygon offset biases shadow depth slightly closer to the camera,
     * preventing z-fighting when the shadow sits just above a surface. */
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    /* Preserve FBO alpha channel: shadow blending contaminates the alpha
     * of the color attachment (SRC_ALPHA blend lowers dst alpha), causing
     * the outline compositing pass to show background bleed-through.
     * Use separate blend: normal RGB blending, but always keep alpha=1. */
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ZERO, GL_ONE);

    /* Quad spans [-1,+1] in local space; scale to shadow extent.
     * When rot_y is non-zero, rotate the shadow quad around its center
     * so it follows the actor on a rotating turnstile platform. */
    float s = extent * scale;
    float model[16];
    if (fabsf(rot_y) > 0.001f) {
        float c = cosf(rot_y), sn = sinf(rot_y);
        memset(model, 0, sizeof(model));
        model[0]  = s * c;
        model[2]  = -s * sn;
        model[5]  = 1.0f;
        model[8]  = s * sn;
        model[10] = s * c;
        model[15] = 1.0f;
        model[12] = x + ps->shadow_offset_x;
        model[13] = y;
        model[14] = z + ps->shadow_offset_z;
    } else {
        memset(model, 0, sizeof(model));
        model[0]  = s;
        model[5]  = 1.0f;
        model[10] = s;
        model[15] = 1.0f;
        model[12] = x + ps->shadow_offset_x;
        model[13] = y;
        model[14] = z + ps->shadow_offset_z;
    }
    shader_set_mat4(shader, "u_model", model);

    glBindVertexArray(ps->shadow_vao);
    glDrawArrays(GL_TRIANGLES, 0, ps->shadow_vertex_count);
    glBindVertexArray(0);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);

    /* Re-enable normal buffer writes */
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    /* Restore standard blend function */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Restore floor lightmap on unit 1 */
    if (ps->diorama_mesh.floor_lm_texture) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ps->diorama_mesh.floor_lm_texture);
        glActiveTexture(GL_TEXTURE0);
    }
}

static void draw_shadow_at_y(const PuzzleScene* ps, GLuint shader,
                              float x, float y, float z, float scale,
                              GLuint shadow_tex, float extent) {
    draw_shadow_at_y_rot(ps, shader, x, y, z, scale, 0.0f, shadow_tex, extent);
}

/* Compute actor Y offset based on current fractional position.
 * If the actor is over a groove tile, lower them by trench_depth.
 * During animation (fractional positions), smoothly interpolate. */
static float actor_groove_y(const PuzzleScene* ps, float col, float row) {
    if (!ps->groove_tile_map || ps->trench_depth <= 0.0f) return 0.0f;

    /* Shift to visual center (same reason as actor_conveyor_y) */
    float cx = col + 0.5f;
    float cz = row + 0.5f;
    int ic = (int)floorf(cx);
    int ir = (int)floorf(cz);

    /* Fraction within tile */
    float fc = cx - (float)ic;
    float fr = cz - (float)ir;

    bool cur = is_groove_tile(ps, ic, ir);

    /* If near a tile boundary, check the adjacent tile for smooth transition */
    bool next_c = (fc > 0.5f) ? is_groove_tile(ps, ic + 1, ir) : is_groove_tile(ps, ic - 1, ir);
    bool next_r = (fr > 0.5f) ? is_groove_tile(ps, ic, ir + 1) : is_groove_tile(ps, ic, ir - 1);

    /* If fully within one tile and groove status won't change, return directly */
    if (cur) return -ps->trench_depth;
    if (!cur && !next_c && !next_r) return 0.0f;

    /* Near a boundary — use smoothstep transition.
     * Transition happens in the 0.3 units around the tile boundary. */
    float y = 0.0f;
    float blend_zone = 0.3f;

    /* Check col-direction boundary */
    if (fc > (1.0f - blend_zone) && is_groove_tile(ps, ic + 1, ir)) {
        float t = (fc - (1.0f - blend_zone)) / blend_zone;
        float s = t * t * (3.0f - 2.0f * t);
        y = -ps->trench_depth * s;
    } else if (fc < blend_zone && is_groove_tile(ps, ic - 1, ir) && !cur) {
        float t = (blend_zone - fc) / blend_zone;
        float s = t * t * (3.0f - 2.0f * t);
        y = -ps->trench_depth * s;
    }

    /* Check row-direction boundary */
    if (fr > (1.0f - blend_zone) && is_groove_tile(ps, ic, ir + 1)) {
        float t = (fr - (1.0f - blend_zone)) / blend_zone;
        float s = t * t * (3.0f - 2.0f * t);
        float ry = -ps->trench_depth * s;
        if (ry < y) y = ry; /* take the lower value */
    } else if (fr < blend_zone && is_groove_tile(ps, ic, ir - 1) && !cur) {
        float t = (blend_zone - fr) / blend_zone;
        float s = t * t * (3.0f - 2.0f * t);
        float ry = -ps->trench_depth * s;
        if (ry < y) y = ry;
    }

    return y;
}

/* Compute actor Y offset for elevated tiles (conveyors and turnstile platforms).
 * Actors standing on an elevated tile are raised by CONVEYOR_HEIGHT. */
#define CONVEYOR_ELEV     0.10f   /* must match diorama_gen.c CONVEYOR_HEIGHT */
#define CONVEYOR_BELT_H   0.006f  /* must match diorama_gen.c CONVEYOR_BELT_H */
#define CONVEYOR_BELT_TOP (CONVEYOR_ELEV + CONVEYOR_BELT_H)  /* shadow plane */
static float actor_conveyor_y(const PuzzleScene* ps, float col, float row) {
    /* Shift to visual center: grid position 5 → actor center at 5.5.
     * Without this, a stationary actor at exact integer coords has fc=0,
     * which triggers edge blending and drops elevation to zero. */
    float cx = col + 0.5f;
    float cz = row + 0.5f;
    int ic = (int)floorf(cx);
    int ir = (int)floorf(cz);
    float fc = cx - (float)ic;
    float fr = cz - (float)ir;

    bool cur = is_elevated_tile(ps, ic, ir);

    /* Simple: if on an elevated tile, raise. With smooth transition near edges. */
    if (cur) {
        /* Check if moving off elevated area — smooth transition */
        float y = CONVEYOR_ELEV;
        float blend = 0.3f;
        /* Check edges for smooth step-down */
        if (fc > (1.0f - blend) && !is_elevated_tile(ps, ic + 1, ir)) {
            float t = (fc - (1.0f - blend)) / blend;
            float s = t * t * (3.0f - 2.0f * t);
            y = CONVEYOR_ELEV * (1.0f - s);
        } else if (fc < blend && !is_elevated_tile(ps, ic - 1, ir)) {
            float t = (blend - fc) / blend;
            float s = t * t * (3.0f - 2.0f * t);
            y = CONVEYOR_ELEV * (1.0f - s);
        }
        if (fr > (1.0f - blend) && !is_elevated_tile(ps, ic, ir + 1)) {
            float t = (fr - (1.0f - blend)) / blend;
            float s = t * t * (3.0f - 2.0f * t);
            float ry = CONVEYOR_ELEV * (1.0f - s);
            if (ry < y) y = ry;
        } else if (fr < blend && !is_elevated_tile(ps, ic, ir - 1)) {
            float t = (blend - fr) / blend;
            float s = t * t * (3.0f - 2.0f * t);
            float ry = CONVEYOR_ELEV * (1.0f - s);
            if (ry < y) y = ry;
        }
        return y;
    }

    /* Not on elevated tile — check if approaching one */
    float y = 0.0f;
    float blend = 0.3f;
    if (fc > (1.0f - blend) && is_elevated_tile(ps, ic + 1, ir)) {
        float t = (fc - (1.0f - blend)) / blend;
        float s = t * t * (3.0f - 2.0f * t);
        y = CONVEYOR_ELEV * s;
    } else if (fc < blend && is_elevated_tile(ps, ic - 1, ir)) {
        float t = (blend - fc) / blend;
        float s = t * t * (3.0f - 2.0f * t);
        y = CONVEYOR_ELEV * s;
    }
    if (fr > (1.0f - blend) && is_elevated_tile(ps, ic, ir + 1)) {
        float t = (fr - (1.0f - blend)) / blend;
        float s = t * t * (3.0f - 2.0f * t);
        float ry = CONVEYOR_ELEV * s;
        if (ry > y) y = ry;
    } else if (fr < blend && is_elevated_tile(ps, ic, ir - 1)) {
        float t = (blend - fr) / blend;
        float s = t * t * (3.0f - 2.0f * t);
        float ry = CONVEYOR_ELEV * s;
        if (ry > y) y = ry;
    }
    return y;
}

static void draw_shadow(const PuzzleScene* ps, GLuint shader,
                         float x, float z, float scale,
                         GLuint shadow_tex, float extent) {
    draw_shadow_at_y(ps, shader, x, 0.01f, z, scale, shadow_tex, extent);
}

/* Multi-plane actor shadow: draws on both floor and conveyor surfaces
 * without double-darkening.  Uses stencil to prevent overlap:
 *   1. Floor shadow (GL_LESS, Y=0.01): writes stencil=1 where it renders.
 *      Passes on floor (Y=0), blocked by belt (Y=0.078 is closer).
 *   2. Conveyor shadow (GL_LESS, Y=belt+eps): only renders where stencil≠1,
 *      i.e. where the floor shadow was blocked by the belt.
 *      Passes on belt, blocked on walls (closer depth). */
static void draw_actor_shadow_multiplane_rot(const PuzzleScene* ps, GLuint shader,
                                              float x, float z, float scale,
                                              float rot_y,
                                              GLuint shadow_tex, float extent) {
    /* --- Floor plane: write stencil=1 wherever the shadow renders --- */
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    draw_shadow_at_y_rot(ps, shader, x, 0.01f, z, scale, rot_y, shadow_tex, extent);

    /* --- Conveyor plane: only where floor shadow didn't reach --- */
    if (ps->conveyor_tile_map) {
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        draw_shadow_at_y_rot(ps, shader,
                         x, CONVEYOR_BELT_TOP + 0.005f,
                         z, scale, rot_y, shadow_tex, extent);
    }

    glDisable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
}

static void draw_actor_shadow_multiplane(const PuzzleScene* ps, GLuint shader,
                                          float x, float z, float scale,
                                          GLuint shadow_tex, float extent) {
    draw_actor_shadow_multiplane_rot(ps, shader, x, z, scale, 0.0f,
                                      shadow_tex, extent);
}

/* Helper: destroy all turnstile meshes (platform + walls + gears) */
static void destroy_turnstile_meshes(PuzzleScene* ps) {
    for (int i = 0; i < ps->turnstile_mesh_count; i++) {
        if (ps->turnstile_meshes[i].valid) {
            voxel_mesh_destroy(&ps->turnstile_meshes[i].mesh);
            for (int g = 0; g < ps->turnstile_meshes[i].gear_count; g++)
                voxel_mesh_destroy(&ps->turnstile_meshes[i].gears[g]);
        }
    }
    ps->turnstile_mesh_count = 0;
}

/* Regenerate a single turnstile's mesh (walls + platform) from current grid state */
static void regenerate_turnstile_mesh(PuzzleScene* ps, int idx) {
    if (ps->turnstile_meshes[idx].valid) {
        voxel_mesh_destroy(&ps->turnstile_meshes[idx].mesh);
        for (int g = 0; g < ps->turnstile_meshes[idx].gear_count; g++)
            voxel_mesh_destroy(&ps->turnstile_meshes[idx].gears[g]);
        ps->turnstile_meshes[idx].valid = false;
    }

    int jc = ps->turnstile_meshes[idx].jc;
    int jr = ps->turnstile_meshes[idx].jr;

    /* Generate walls + raised platform */
    voxel_mesh_begin(&ps->turnstile_meshes[idx].mesh);
    diorama_generate_turnstile(&ps->turnstile_meshes[idx].mesh,
                                ps->grid, &ps->cached_biome,
                                (const int (*)[2])ps->turnstile_meshes[idx].cells, 4,
                                jc, jr);
    voxel_mesh_build(&ps->turnstile_meshes[idx].mesh, 0.0625f);

    /* Generate gear meshes — 1 central + 4 satellites */
    float cx = (float)jc;  /* junction is at corner of 4 tiles */
    float cz = (float)jr;
    int gc = 0;

    /* Central gear at junction point */
    ps->turnstile_meshes[idx].gear_cx[gc] = cx;
    ps->turnstile_meshes[idx].gear_cz[gc] = cz;
    ps->turnstile_meshes[idx].gear_speed[gc] = 1.0f;  /* 1:1 with platform */
    voxel_mesh_begin(&ps->turnstile_meshes[idx].gears[gc]);
    diorama_generate_gear(&ps->turnstile_meshes[idx].gears[gc],
                           cx, cz, 8, 0.25f);
    voxel_mesh_build(&ps->turnstile_meshes[idx].gears[gc], 0.0625f);
    gc++;

    /* Satellite gears under the exposed corners of the 12-gon.
     * Randomly sized and positioned like watch clockwork.  A simple
     * deterministic hash from (jc, jr, index) seeds the variation so
     * each turnstile looks unique but consistent across rebuilds. */
    struct {
        float dx, dz;       /* offset from junction */
        float radius;       /* gear radius */
        int   teeth;        /* tooth count */
        float speed;        /* rotation speed multiplier */
    } sats[] = {
        /* Primary gear in each corner (larger) */
        { -0.80f, -0.80f,  0.22f, 10, -1.4f },
        {  0.80f, -0.80f,  0.19f,  8, -1.8f },
        {  0.80f,  0.80f,  0.24f, 11, -1.2f },
        { -0.80f,  0.80f,  0.17f,  7, -2.0f },
        /* Smaller secondary gears clustered near primaries */
        { -0.55f, -0.95f,  0.12f,  6,  2.5f },
        {  0.95f, -0.55f,  0.10f,  5,  3.0f },
        {  0.55f,  0.95f,  0.13f,  6,  2.2f },
        { -0.95f,  0.55f,  0.11f,  5,  2.8f },
        /* Tiny accent gears */
        { -0.60f, -0.60f,  0.08f,  5,  3.5f },
        {  0.62f, -0.62f,  0.07f,  4, -4.0f },
        {  0.58f,  0.63f,  0.09f,  5,  3.2f },
    };
    int sat_count = (int)(sizeof(sats) / sizeof(sats[0]));
    if (gc + sat_count > TURNSTILE_GEAR_COUNT)
        sat_count = TURNSTILE_GEAR_COUNT - gc;

    for (int s = 0; s < sat_count; s++) {
        float gx = cx + sats[s].dx;
        float gz = cz + sats[s].dz;
        ps->turnstile_meshes[idx].gear_cx[gc] = gx;
        ps->turnstile_meshes[idx].gear_cz[gc] = gz;
        ps->turnstile_meshes[idx].gear_speed[gc] = sats[s].speed;
        voxel_mesh_begin(&ps->turnstile_meshes[idx].gears[gc]);
        diorama_generate_gear(&ps->turnstile_meshes[idx].gears[gc],
                               gx, gz, sats[s].teeth, sats[s].radius);
        voxel_mesh_build(&ps->turnstile_meshes[idx].gears[gc], 0.0625f);
        gc++;
    }
    ps->turnstile_meshes[idx].gear_count = gc;

    ps->turnstile_meshes[idx].valid = true;
}

static void build_diorama(PuzzleScene* ps) {
    if (ps->diorama_built) {
        voxel_mesh_destroy(&ps->diorama_mesh);
        actor_render_destroy(&ps->theseus_parts);
        actor_render_destroy(&ps->minotaur_parts);
        voxel_mesh_destroy(&ps->groove_box_mesh);
        destroy_shadow_resources(ps);
        destroy_turnstile_meshes(ps);
        free(ps->groove_tile_map);
        ps->groove_tile_map = NULL;
        free(ps->conveyor_tile_map);
        ps->conveyor_tile_map = NULL;
        free(ps->turnstile_tile_map);
        ps->turnstile_tile_map = NULL;
    }

    int cols = ps->grid->cols;
    int rows = ps->grid->rows;

    /* Load biome config */
    BiomeConfig biome;
    biome_config_defaults(&biome);

    if (ps->grid->biome[0] != '\0') {
        char biome_path[512];
        snprintf(biome_path, sizeof(biome_path), "%s/assets/biomes/%s.json",
                 platform_get_asset_dir(), ps->grid->biome);
        biome_config_load(&biome, biome_path);
    }
    ps->cached_biome = biome;  /* cache for turnstile mesh regeneration */

    /* Cache trench depth and build groove tile map for actor Y adjustment */
    ps->trench_depth = biome.groove_trench.trench_depth;
    ps->groove_map_cols = cols;
    ps->groove_map_rows = rows;
    ps->groove_tile_map = calloc((size_t)(cols * rows), sizeof(bool));
    if (ps->groove_tile_map) {
        for (int fi = 0; fi < ps->grid->feature_count; fi++) {
            const Feature* feat = ps->grid->features[fi];
            if (!feat) continue;
            int path_cols[32], path_rows[32];
            int path_len = groove_box_get_path(feat, path_cols, path_rows, 32);
            for (int pi = 0; pi < path_len; pi++) {
                int pc = path_cols[pi], pr = path_rows[pi];
                if (pc >= 0 && pc < cols && pr >= 0 && pr < rows)
                    ps->groove_tile_map[pr * cols + pc] = true;
            }
        }
    }

    /* Build conveyor tile map for actor elevation */
    ps->conveyor_map_cols = cols;
    ps->conveyor_map_rows = rows;
    ps->conveyor_tile_map = calloc((size_t)(cols * rows), sizeof(bool));
    if (ps->conveyor_tile_map) {
        for (int fi = 0; fi < ps->grid->feature_count; fi++) {
            const Feature* feat = ps->grid->features[fi];
            if (!feat) continue;
            if (conveyor_get_direction(feat) == DIR_NONE) continue;
            int cc = feat->col, cr = feat->row;
            if (cc >= 0 && cc < cols && cr >= 0 && cr < rows)
                ps->conveyor_tile_map[cr * cols + cc] = true;
        }
    }

    /* Identify auto-turnstile features and build exclusion set */
    DioramaExcludeSet exclude;
    exclude.count = 0;
    ps->turnstile_mesh_count = 0;

    for (int fi = 0; fi < ps->grid->feature_count; fi++) {
        const Feature* feat = ps->grid->features[fi];
        if (!feat) continue;
        int jc, jr;
        bool cw;
        if (!auto_turnstile_get_junction(feat, &jc, &jr, &cw)) continue;
        if (ps->turnstile_mesh_count >= MAX_AUTO_TURNSTILES) break;

        int idx = ps->turnstile_mesh_count++;
        ps->turnstile_meshes[idx].jc = jc;
        ps->turnstile_meshes[idx].jr = jr;
        ps->turnstile_meshes[idx].clockwise = cw;
        ps->turnstile_meshes[idx].valid = false;
        ps->turnstile_meshes[idx].held_angle = 0.0f;

        /* 4 tiles: NW, NE, SE, SW */
        ps->turnstile_meshes[idx].cells[0][0] = jc - 1;
        ps->turnstile_meshes[idx].cells[0][1] = jr;
        ps->turnstile_meshes[idx].cells[1][0] = jc;
        ps->turnstile_meshes[idx].cells[1][1] = jr;
        ps->turnstile_meshes[idx].cells[2][0] = jc;
        ps->turnstile_meshes[idx].cells[2][1] = jr - 1;
        ps->turnstile_meshes[idx].cells[3][0] = jc - 1;
        ps->turnstile_meshes[idx].cells[3][1] = jr - 1;

        /* Add these 4 cells to the exclusion set */
        for (int ci = 0; ci < 4; ci++) {
            if (exclude.count < 32) {
                exclude.cells[exclude.count][0] = ps->turnstile_meshes[idx].cells[ci][0];
                exclude.cells[exclude.count][1] = ps->turnstile_meshes[idx].cells[ci][1];
                exclude.count++;
            }
        }
    }

    /* Build turnstile tile map for actor elevation (same height as conveyors) */
    ps->turnstile_map_cols = cols;
    ps->turnstile_map_rows = rows;
    ps->turnstile_tile_map = calloc((size_t)(cols * rows), sizeof(bool));
    if (ps->turnstile_tile_map) {
        for (int i = 0; i < exclude.count; i++) {
            int tc = exclude.cells[i][0];
            int tr = exclude.cells[i][1];
            if (tc >= 0 && tc < cols && tr >= 0 && tr < rows)
                ps->turnstile_tile_map[tr * cols + tc] = true;
        }
    }

    /* Generate diorama via procedural pipeline (excluding turnstile cells) */
    voxel_mesh_begin(&ps->diorama_mesh);

    DioramaGenResult gen_result;
    diorama_generate_ex(&ps->diorama_mesh, ps->grid, &biome, &gen_result,
                         exclude.count > 0 ? &exclude : NULL);

    /* Generate floor shadow lightmap before building the mesh */
    {
        FloorLightmap floor_lm;
        floor_lightmap_generate(&floor_lm,
                                 ps->diorama_mesh.boxes, ps->diorama_mesh.box_count,
                                 gen_result.grid_cols, gen_result.grid_rows,
                                 &biome.floor_shadow);
        /* Pass universal shadow softness to mesh for wall heuristic */
        ps->diorama_mesh.shadow_softness = biome.floor_shadow.shadow_softness;
        /* Cache wall style for shader uniforms at render time */
        ps->wall_style = biome.wall_style;

        if (floor_lm.texture) {
            voxel_mesh_set_floor_lightmap(&ps->diorama_mesh, floor_lm.texture,
                                           floor_lm.origin_x, floor_lm.origin_z,
                                           floor_lm.extent_x, floor_lm.extent_z,
                                           gen_result.grid_cols, gen_result.grid_rows);
            /* Don't call floor_lightmap_destroy — mesh took ownership of texture */
        }
    }

    voxel_mesh_build(&ps->diorama_mesh, 0.0625f);

    /* Generate separate wall meshes for each auto-turnstile */
    for (int i = 0; i < ps->turnstile_mesh_count; i++) {
        regenerate_turnstile_mesh(ps, i);
    }
    /* Set up camera */
    diorama_camera_init(&ps->diorama_cam, cols, rows);
    diorama_camera_set_target(&ps->diorama_cam,
                               cols * 0.5f, 0.0f, rows * 0.5f);

    /* Set up lighting with generated point lights */
    lighting_init(&ps->diorama_light);
    for (int i = 0; i < gen_result.light_count; i++) {
        lighting_add_point(&ps->diorama_light,
                           gen_result.lights[i].pos[0],
                           gen_result.lights[i].pos[1],
                           gen_result.lights[i].pos[2],
                           gen_result.lights[i].color[0],
                           gen_result.lights[i].color[1],
                           gen_result.lights[i].color[2],
                           gen_result.lights[i].radius);
    }

    /* Build actor meshes via actor_render module.
     * Meshes are centered at origin XZ, bottom at Y=0.
     * Each includes a ground-plane occluder for AO baking. */
    actor_render_build_theseus(&ps->theseus_parts);
    actor_render_build_minotaur(&ps->minotaur_parts);

    /* Groove box — wooden crate */
    {
        /* Wall thickness matches diorama_gen.c WALL_THICKNESS (0.20).
         * Box fills tile minus wall inset on each side. */
        float inset = 0.12f;
        float box_sz = 1.0f - 2.0f * inset;
        float box_h  = 0.45f + ps->trench_depth;
        float half   = box_sz * 0.5f;
        voxel_mesh_begin(&ps->groove_box_mesh);
        /* Main crate body */
        voxel_mesh_add_box(&ps->groove_box_mesh,
                            -half, 0.0f, -half,
                            box_sz, box_h, box_sz,
                            0.55f, 0.40f, 0.25f, 1.0f,
                            true);
        /* Darker trim bands on top edges */
        float trim = 0.04f;
        float trim_h = 0.03f;
        /* North/south trim */
        voxel_mesh_add_box(&ps->groove_box_mesh,
                            -half, box_h - trim_h, -half,
                            box_sz, trim_h, trim,
                            0.40f, 0.28f, 0.16f, 1.0f, true);
        voxel_mesh_add_box(&ps->groove_box_mesh,
                            -half, box_h - trim_h, half - trim,
                            box_sz, trim_h, trim,
                            0.40f, 0.28f, 0.16f, 1.0f, true);
        /* East/west trim */
        voxel_mesh_add_box(&ps->groove_box_mesh,
                            -half, box_h - trim_h, -half,
                            trim, trim_h, box_sz,
                            0.40f, 0.28f, 0.16f, 1.0f, true);
        voxel_mesh_add_box(&ps->groove_box_mesh,
                            half - trim, box_h - trim_h, -half,
                            trim, trim_h, box_sz,
                            0.40f, 0.28f, 0.16f, 1.0f, true);
        /* Ground plane occluder for AO bake */
        voxel_mesh_add_occluder(&ps->groove_box_mesh,
                                 -2.0f, -0.05f, -2.0f,
                                 4.0f, 0.05f, 4.0f);
        voxel_mesh_build(&ps->groove_box_mesh, box_sz * 0.25f);
    }

    /* Precomputed blurred rectangular shadow textures + quad.
     * Uses the same blur/softness/intensity as floor wall shadows. */
    build_shadow_resources(ps, &biome.actor_shadow);

    ps->diorama_built = true;

    /* Initialize dust puff particle system */
    dust_puff_init();

    LOG_INFO("Diorama built: %d static verts, theseus %d verts, minotaur %d+%d verts",
             voxel_mesh_get_vertex_count(&ps->diorama_mesh),
             voxel_mesh_get_vertex_count(&ps->theseus_parts.body),
             voxel_mesh_get_vertex_count(&ps->minotaur_parts.body),
             voxel_mesh_get_vertex_count(&ps->minotaur_parts.horns));
}

static void render_diorama(PuzzleScene* ps, int vw, int vh) {
    if (!ps->diorama_built) return;

    GLuint shader = renderer_get_voxel_shader();
    if (!shader) return;

    /* Apply camera shake offset (temporarily shifts the camera target) */
    float saved_target[3];
    saved_target[0] = ps->diorama_cam.target[0];
    saved_target[1] = ps->diorama_cam.target[1];
    saved_target[2] = ps->diorama_cam.target[2];
    if (ps->shake_timer > 0.0f) {
        ps->diorama_cam.target[0] += ps->shake_offset_x;
        ps->diorama_cam.target[2] += ps->shake_offset_z;
    }

    /* Update camera for current viewport */
    diorama_camera_update(&ps->diorama_cam, vw, vh);

    /* Enable depth test for 3D rendering */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);

    shader_use(shader);

    /* Set VP matrix */
    shader_set_mat4(shader, "u_vp", diorama_camera_get_vp(&ps->diorama_cam));

    /* Model matrix = identity (diorama is in world space) */
    float identity[16];
    memset(identity, 0, sizeof(identity));
    identity[0] = identity[5] = identity[10] = identity[15] = 1.0f;
    shader_set_mat4(shader, "u_model", identity);

    /* Apply lighting */
    lighting_apply(&ps->diorama_light, shader);

    /* Cel-shading toggle */
    shader_set_int(shader, "u_cel_shading", g_settings.cel_shading ? 1 : 0);

    /* Set AO texture uniform for static diorama mesh */
    shader_set_int(shader, "u_ao_texture", 0);  /* texture unit 0 */
    shader_set_int(shader, "u_has_ao", voxel_mesh_has_ao(&ps->diorama_mesh) ? 1 : 0);
    shader_set_float(shader, "u_ao_intensity", 1.0f);

    /* Set floor lightmap uniforms */
    shader_set_int(shader, "u_floor_lightmap", 1);  /* texture unit 1 */
    if (ps->diorama_mesh.floor_lm_texture) {
        shader_set_vec4(shader, "u_lightmap_bounds",
                         ps->diorama_mesh.floor_lm_origin_x,
                         ps->diorama_mesh.floor_lm_origin_z,
                         ps->diorama_mesh.floor_lm_extent_x,
                         ps->diorama_mesh.floor_lm_extent_z);
    }

    /* Set wall stone texture uniforms from biome config */
    {
        const WallStyle* ws = &ps->wall_style;
        shader_set_vec4(shader, "u_wall_stone_a",
                         ws->stone_height, ws->stone_width_min,
                         ws->stone_width_max, ws->mortar_width);
        shader_set_vec4(shader, "u_wall_stone_b",
                         ws->mortar_darkness, ws->bevel_width,
                         ws->bevel_darkness, ws->color_variation);
        shader_set_vec4(shader, "u_wall_stone_c",
                         ws->grain_intensity, ws->grain_scale, ws->wear, 0.0f);
        shader_set_vec4(shader, "u_wall_stone_d",
                         ws->gap_color[0], ws->gap_color[1], ws->gap_color[2], 0.0f);
    }

    /* Conveyor belt scroll animation.
     * During a CONVEYOR_PUSH environment event, scroll = effect_progress
     * maps to one tile of belt movement. */
    {
        float scroll = 0.0f;
        float dir_x = 1.0f, dir_z = 0.0f;
        if (anim_queue_is_playing(&ps->anim)) {
            const AnimEvent* cur = anim_queue_current_event(&ps->anim);
            AnimPhase phase = anim_queue_phase(&ps->anim);
            if (cur && phase == ANIM_PHASE_ENVIRONMENT &&
                cur->type == ANIM_EVT_CONVEYOR_PUSH) {
                scroll = anim_queue_effect_progress(&ps->anim);
                /* Convert direction enum to world XZ vector */
                switch (cur->conveyor.direction) {
                    case DIR_EAST:  dir_x =  1.0f; dir_z =  0.0f; break;
                    case DIR_WEST:  dir_x = -1.0f; dir_z =  0.0f; break;
                    case DIR_NORTH: dir_x =  0.0f; dir_z =  1.0f; break;
                    case DIR_SOUTH: dir_x =  0.0f; dir_z = -1.0f; break;
                    default: break;
                }
            }
        }
        shader_set_float(shader, "u_conveyor_scroll", scroll);
        shader_set_vec2(shader, "u_conveyor_dir", dir_x, dir_z);
    }

    /* Draw static geometry (floor, walls, exit marker) */
    voxel_mesh_draw(&ps->diorama_mesh);

    /* Draw auto-turnstile: gears first, then pit shadow, then plate+walls.
     * Order matters because the plate (Y=0.10) would block the pit shadow
     * (Y=-0.004) in the depth buffer if drawn first. */
    float turnstile_angles[MAX_AUTO_TURNSTILES];
    for (int ti = 0; ti < ps->turnstile_mesh_count; ti++) {
        if (!ps->turnstile_meshes[ti].valid) { turnstile_angles[ti] = 0; continue; }

        int jc = ps->turnstile_meshes[ti].jc;
        int jr = ps->turnstile_meshes[ti].jr;
        bool cw = ps->turnstile_meshes[ti].clockwise;

        /* Determine rotation angle from animation or held pose */
        float platform_angle = ps->turnstile_meshes[ti].held_angle;
        if (anim_queue_is_playing(&ps->anim)) {
            const AnimEvent* cur = anim_queue_current_event(&ps->anim);
            AnimPhase phase = anim_queue_phase(&ps->anim);
            if (cur && (phase == ANIM_PHASE_ENVIRONMENT) &&
                cur->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE &&
                cur->turnstile.junction_col == jc &&
                cur->turnstile.junction_row == jr) {
                float raw_t = anim_queue_rotation_progress(&ps->anim);
                float target = cw ? (float)M_PI_2 : -(float)M_PI_2;

                if (anim_queue_is_reversing(&ps->anim)) {
                    platform_angle = target * (raw_t - 1.0f);
                } else {
                    if (raw_t < 0.85f) {
                        platform_angle = target * (raw_t / 0.85f);
                    } else {
                        float u = (raw_t - 0.85f) / 0.15f;
                        float osc = sinf(u * (float)M_PI * 2.0f)
                                  * (1.0f - u) * 0.04f;
                        platform_angle = target * (1.0f + osc);
                    }
                }
            }
        }
        turnstile_angles[ti] = platform_angle;

        /* Draw gears — each rotates around its own center axis */
        for (int g = 0; g < ps->turnstile_meshes[ti].gear_count; g++) {
            float gear_angle = platform_angle * ps->turnstile_meshes[ti].gear_speed[g];
            float gcx = ps->turnstile_meshes[ti].gear_cx[g];
            float gcz = ps->turnstile_meshes[ti].gear_cz[g];

            float t1[16], ry[16], t2[16], tmp[16], model[16];
            mat4_translate(t1, gcx, 0.0f, gcz);
            mat4_rot_y(ry, gear_angle);
            mat4_translate(t2, -gcx, 0.0f, -gcz);
            mat4_mul(tmp, t1, ry);
            mat4_mul(model, tmp, t2);

            shader_set_mat4(shader, "u_model", model);
            voxel_mesh_draw(&ps->turnstile_meshes[ti].gears[g]);
        }
    }

    /* Draw vignette shadow over each turnstile gear pit.
     * Rendered AFTER gears (so shadow darkens them) but BEFORE the
     * turnstile plate+walls (so the plate at Y=0.10 draws on top). */
    if (ps->turnstile_mesh_count > 0) {
        shader_set_mat4(shader, "u_model", identity);
    }
    if (ps->turnstile_mesh_count > 0 && ps->shadow_tex_pit) {
        float saved_ox = ps->shadow_offset_x;
        float saved_oz = ps->shadow_offset_z;
        ((PuzzleScene*)ps)->shadow_offset_x = 0.0f;
        ((PuzzleScene*)ps)->shadow_offset_z = 0.0f;
        for (int ti = 0; ti < ps->turnstile_mesh_count; ti++) {
            if (!ps->turnstile_meshes[ti].valid) continue;
            int jc = ps->turnstile_meshes[ti].jc;
            int jr = ps->turnstile_meshes[ti].jr;
            draw_shadow_at_y(ps, shader,
                             (float)jc, -0.004f, (float)jr,
                             1.0f, ps->shadow_tex_pit, ps->shadow_extent_pit);
        }
        ((PuzzleScene*)ps)->shadow_offset_x = saved_ox;
        ((PuzzleScene*)ps)->shadow_offset_z = saved_oz;
    }

    /* Draw turnstile platform + walls on top of shadows */
    for (int ti = 0; ti < ps->turnstile_mesh_count; ti++) {
        if (!ps->turnstile_meshes[ti].valid) continue;
        int jc = ps->turnstile_meshes[ti].jc;
        int jr = ps->turnstile_meshes[ti].jr;

        float t1[16], ry[16], t2[16], tmp[16], model[16];
        mat4_translate(t1, (float)jc, 0.0f, (float)jr);
        mat4_rot_y(ry, turnstile_angles[ti]);
        mat4_translate(t2, -(float)jc, 0.0f, -(float)jr);
        mat4_mul(tmp, t1, ry);
        mat4_mul(model, tmp, t2);

        shader_set_mat4(shader, "u_model", model);
        voxel_mesh_draw(&ps->turnstile_meshes[ti].mesh);
    }

    /* Reset model matrix back to identity */
    if (ps->turnstile_mesh_count > 0) {
        shader_set_mat4(shader, "u_model", identity);
    }

    /* Blending is already enabled globally (engine default) for shadow alpha */

    /* Draw dynamic groove boxes */
    {
        bool animating = anim_queue_is_playing(&ps->anim);

        /* During a push animation, one groove box is interpolated via aux tweens.
         * Grid state is already updated (box at destination), so we match
         * the animated box by its destination (to) position. */
        bool box_animating = false;
        int anim_box_to_col = -1, anim_box_to_row = -1;
        float anim_box_col = 0.0f, anim_box_row = 0.0f;

        if (animating &&
            anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
            anim_queue_theseus_event_type(&ps->anim) == ANIM_EVT_BOX_SLIDE) {
            /* Find the BOX_SLIDE event to get the "to" position for matching */
            const TurnRecord* rec = &ps->anim.record;
            for (int ei = 0; ei < rec->event_count; ei++) {
                if (rec->events[ei].type == ANIM_EVT_BOX_SLIDE) {
                    anim_box_to_col = rec->events[ei].box.box_to_col;
                    anim_box_to_row = rec->events[ei].box.box_to_row;
                    break;
                }
            }

            /* Compute piecewise box position — synchronized with Theseus:
             * Phase 1 (0.00–0.20): approach — box stays at from position
             * Phase 2 (0.20–0.75): push — box slides from→to (same smoothstep as Theseus)
             * Phase 3 (0.75–1.00): settle — box at to position */
            float t = tween_progress(&ps->anim.aux_x);
            float box_from_col = ps->anim.aux_x.start;
            float box_from_row = ps->anim.aux_y.start;
            float box_to_col   = ps->anim.aux_x.end;
            float box_to_row   = ps->anim.aux_y.end;

            if (t < 0.20f) {
                anim_box_col = box_from_col;
                anim_box_row = box_from_row;
            } else if (t < 0.75f) {
                float u = (t - 0.20f) / 0.55f;
                float s = u * u * (3.0f - 2.0f * u); /* smoothstep */
                anim_box_col = box_from_col + (box_to_col - box_from_col) * s;
                anim_box_row = box_from_row + (box_to_row - box_from_row) * s;
            } else {
                anim_box_col = box_to_col;
                anim_box_row = box_to_row;
            }
            box_animating = true;
        }

        shader_set_int(shader, "u_has_ao",
                       voxel_mesh_has_ao(&ps->groove_box_mesh) ? 1 : 0);
        shader_set_float(shader, "u_ao_intensity", 1.0f);

        for (int fi = 0; fi < ps->grid->feature_count; fi++) {
            const Feature* feat = ps->grid->features[fi];
            if (!feat || !feat->vt || !feat->vt->name) continue;
            if (strcmp(feat->vt->name, "groove_box") != 0) continue;

            /* GrooveBoxData layout: first two ints are groove_cols[32],
             * groove_rows[32], groove_length, then box_col, box_row.
             * But that's the internal struct — we can also just read
             * feat->col/feat->row which are kept in sync with box position. */
            int box_col = feat->col;
            int box_row = feat->row;

            float render_col, render_row;

            /* Check if this is the box currently being animated.
             * Grid already moved box to destination, so match on to-position. */
            if (box_animating &&
                box_col == anim_box_to_col && box_row == anim_box_to_row) {
                render_col = anim_box_col;
                render_row = anim_box_row;
            } else {
                render_col = (float)box_col;
                render_row = (float)box_row;
            }

            /* Draw ground shadow under groove box on both levels:
             * 1. Floor rim level — use GL_GREATER depth test with shadow
             *    placed halfway between rim (Y=0) and trench floor (Y=-depth).
             *    GL_GREATER passes where depth buffer has rim surface (closer
             *    to camera than shadow), fails over trench opening (further).
             *    Using the midpoint maximizes depth gap to avoid z-fighting.
             * 2. Trench floor level — normal depth test. */
            float box_y = -ps->trench_depth;
            float rim_shadow_y = -ps->trench_depth * 0.5f;
            glDepthFunc(GL_GREATER);
            draw_shadow_at_y(ps, shader,
                             render_col + 0.5f, rim_shadow_y,
                             render_row + 0.5f, 1.0f,
                             ps->shadow_tex_groovebox, ps->shadow_extent_gb);
            glDepthFunc(GL_LESS);
            draw_shadow_at_y(ps, shader,
                             render_col + 0.5f, box_y + 0.01f,
                             render_row + 0.5f, 1.0f,
                             ps->shadow_tex_groovebox, ps->shadow_extent_gb);

            float model[16];
            memset(model, 0, sizeof(model));
            model[0]  = 1.0f;
            model[5]  = 1.0f;
            model[10] = 1.0f;
            model[15] = 1.0f;
            model[12] = render_col + 0.5f;
            model[13] = box_y;
            model[14] = render_row + 0.5f;
            shader_set_mat4(shader, "u_model", model);
            voxel_mesh_draw(&ps->groove_box_mesh);
        }

        /* Reset model matrix */
        {
            float ident[16];
            memset(ident, 0, sizeof(ident));
            ident[0] = ident[5] = ident[10] = ident[15] = 1.0f;
            shader_set_mat4(shader, "u_model", ident);
        }
    }

    /* Draw dynamic actors with per-frame model matrices, AO, and shadows */
    {
        bool animating = anim_queue_is_playing(&ps->anim);

        /* Minotaur — rolls (tumbles) during movement */
        {
            float mcol, mrow;
            if (animating) {
                anim_queue_minotaur_pos(&ps->anim, &mcol, &mrow);
            } else {
                mcol = (float)ps->grid->minotaur_col;
                mrow = (float)ps->grid->minotaur_row;
            }

            /* Compute groove Y offset for minotaur (conveyor elevation + groove trench) */
            float mino_gy = actor_groove_y(ps, mcol, mrow)
                          + actor_conveyor_y(ps, mcol, mrow);

            /* Compute turnstile platform rotation for minotaur (shared by
             * shadow + body).  Same 85/15 easing as platform mesh. */
            float mino_turnstile_yaw = 0.0f;
            if (animating && anim_queue_phase(&ps->anim) == ANIM_PHASE_ENVIRONMENT) {
                const AnimEvent* env_cur = anim_queue_current_event(&ps->anim);
                if (env_cur && env_cur->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE &&
                    env_cur->turnstile.actor_moved[1]) {
                    float raw_t = anim_queue_rotation_progress(&ps->anim);
                    bool cw_dir = env_cur->turnstile.clockwise;
                    float target = cw_dir ? (float)M_PI_2 : -(float)M_PI_2;
                    if (anim_queue_is_reversing(&ps->anim)) {
                        mino_turnstile_yaw = target * (raw_t - 1.0f);
                    } else if (raw_t < 0.85f) {
                        mino_turnstile_yaw = target * (raw_t / 0.85f);
                    } else {
                        float u = (raw_t - 0.85f) / 0.15f;
                        float osc = sinf(u * (float)M_PI * 2.0f)
                                  * (1.0f - u) * 0.04f;
                        mino_turnstile_yaw = target * (1.0f + osc);
                    }
                }
            }

            /* Compute shadow scale: baseline shrink so the minotaur's shadow
             * extends a similar distance from his body as Theseus's does,
             * plus gentle additional shrink when airborne during the roll arc. */
            float mino_shadow_scale = 0.85f;  /* baseline: tighten around body */
            {
                bool in_mino_roll = animating &&
                    (anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP1 ||
                     anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP2) &&
                    ps->anim.record.minotaur_steps > 0;
                if (in_mino_roll) {
                    int dc = 0, dr = 0;
                    anim_queue_minotaur_dir(&ps->anim, &dc, &dr);
                    if (dc != 0 || dr != 0) {
                        float rt_raw = anim_queue_minotaur_progress(&ps->anim);
                        if (rt_raw >= 0.08f) {
                            float rt = (rt_raw - 0.08f) / (1.0f - 0.08f);
                            float arc = 0.10f * 4.0f * rt * (1.0f - rt);
                            /* Gentle shrink during tumble: ~15% at arc peak */
                            mino_shadow_scale *= (1.0f - arc * 1.5f);
                        }
                    }
                }
            }

            /* Multi-plane shadow on floor + conveyor belt (stencil prevents
             * double-darkening; GL_LESS blocks shadows on walls). */
            draw_actor_shadow_multiplane_rot(ps, shader,
                mcol + 0.5f, mrow + 0.5f, mino_shadow_scale,
                mino_turnstile_yaw,
                ps->shadow_tex_minotaur, ps->shadow_extent_m);

            /* ── Roll animation state ── */
            float mino_half = MINOTAUR_SIZE_FRAC * 0.5f;
            bool in_mino_phase = animating &&
                           (anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP1 ||
                            anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP2);
            int dir_c = 0, dir_r = 0;
            float roll_t = 0.0f;
            if (in_mino_phase) {
                anim_queue_minotaur_dir(&ps->anim, &dir_c, &dir_r);
                roll_t = anim_queue_minotaur_progress(&ps->anim);
            }
            bool actually_moving = in_mino_phase && (dir_c != 0 || dir_r != 0);
            /* in_roll: true only when the minotaur has real steps in this turn.
             * Zero-step turns still run placeholder 0.001s tweens through the
             * minotaur phases, but those shouldn't hide horns or trigger roll. */
            bool in_roll = in_mino_phase && ps->anim.record.minotaur_steps > 0;

            /* ── Deformation state ── */
            DeformState mino_deform;
            deform_state_identity(&mino_deform);

            /* ── Build model matrix ── */
            float model[16];

            if (actually_moving) {
                /* Roll phases (true cube — no unwind needed):
                 * 0.00–0.08: anticipation squat (loading up)
                 * 0.08–1.00: 90° roll around leading bottom edge
                 * At 90° the cube lands on an identical face — done. */
                float start_col = ps->anim.mino_x.start;
                float start_row = ps->anim.mino_y.start;

                if (roll_t < 0.08f) {
                    /* ── Anticipation squat ── */
                    float u = roll_t / 0.08f;
                    float s = u * u * (3.0f - 2.0f * u); /* smoothstep */
                    mino_deform.squash = 1.0f - 0.08f * s;  /* → 0.92 */
                    mino_deform.flare  = 0.04f * s;

                    /* Stay at start position during squat, with facing rotation */
                    {
                        float t_pos[16], ry_mat[16];
                        mat4_translate(t_pos, start_col + 0.5f, mino_gy, start_row + 0.5f);
                        mat4_rot_y(ry_mat, ps->mino_facing_angle);
                        mat4_mul(model, t_pos, ry_mat);
                    }

                } else {
                    /* ── Roll phase: 90° rotation around leading bottom edge ── */
                    float rt = (roll_t - 0.08f) / (1.0f - 0.08f); /* 0→1 */
                    /* Ease-out for a satisfying deceleration into landing */
                    float et = 1.0f - (1.0f - rt) * (1.0f - rt);
                    float angle = et * (float)M_PI * 0.5f; /* 0→90° */

                    /* Arc lift: slight upward offset to prevent floor clipping */
                    float arc_y = 0.10f * 4.0f * rt * (1.0f - rt);

                    /* Pivot is at the leading bottom edge of the start position.
                     * In local space the body center is at (0, 0, 0) (mesh base),
                     * and the pivot is at (dir_c * half, 0, dir_r * half). */
                    float px = (float)dir_c * mino_half;
                    float pz = (float)dir_r * mino_half;

                    /* World position of the pivot (fixed throughout the roll) */
                    float wx = start_col + 0.5f + px;
                    float wy = mino_gy;
                    float wz = start_row + 0.5f + pz;

                    /* Residual: compensate for tile/body size mismatch.
                     * After 90° roll, the new bottom-face center (originally
                     * the +X face at local (half, half, 0)) ends up at
                     * pivot_x + 0.325 via rotation. Total displacement from
                     * start center = half (pivot offset) + half (rotation) = size.
                     * Tile is 1.0 wide, so residual covers the remaining
                     * (1.0 - size) = 0.35 gap. */
                    float residual = et * (1.0f - MINOTAUR_SIZE_FRAC);

                    /* Build model: T(world_pivot + residual + arc) * R(angle) * T(-local_pivot) */
                    float t1[16], rot[16], t2[16], tmp[16];

                    mat4_translate(t1,
                                   wx + (float)dir_c * residual,
                                   wy + arc_y,
                                   wz + (float)dir_r * residual);

                    /* Rotation axis depends on movement direction:
                     * Moving ±X → rotate around Z axis
                     * Moving ±Z → rotate around X axis
                     * Sign: rolling "forward" = clockwise from the direction of travel */
                    if (dir_c != 0) {
                        mat4_rot_z(rot, -(float)dir_c * angle);
                    } else {
                        mat4_rot_x(rot, (float)dir_r * angle);
                    }

                    mat4_translate(t2, -px, 0.0f, -pz);

                    /* model = t1 * rot * t2 */
                    mat4_mul(tmp, t1, rot);
                    mat4_mul(model, tmp, t2);
                }
            } else {
                /* ── Idle or non-moving step: translation + facing rotation ── */

                /* Build model: T(pos) * Ry(facing + turnstile) */
                float total_yaw = ps->mino_facing_angle + mino_turnstile_yaw;
                {
                    float t_pos[16], ry_mat[16], tmp_m[16];
                    mat4_translate(t_pos, mcol + 0.5f, mino_gy, mrow + 0.5f);
                    mat4_rot_y(ry_mat, total_yaw);
                    mat4_mul(model, t_pos, ry_mat);
                }

                /* Post-roll wobble (heavier than Theseus) */
                if (ps->mino_wobble_active) {
                    float wt = ps->mino_wobble_timer;
                    float amplitude = 0.06f;
                    float freq = 20.0f;
                    float damping = 14.0f;
                    float wobble = amplitude * sinf(wt * freq) * expf(-wt * damping);
                    mino_deform.squash = 1.0f + wobble;
                    mino_deform.flare  = (1.0f - mino_deform.squash) * 0.4f;
                    if (mino_deform.flare < 0.0f) mino_deform.flare = 0.0f;
                }
            }

            /* ── Draw body ── */
            shader_set_mat4(shader, "u_model", model);
            shader_set_float(shader, "u_deform_height",
                             -ps->minotaur_parts.body_height);
            /* World-space actor AO: ground Y + mesh height so the shadow
             * band tracks the floor contact even during roll rotation. */
            shader_set_float(shader, "u_actor_ground_y", mino_gy);
            shader_set_float(shader, "u_actor_height",
                             ps->minotaur_parts.body_height);
            deform_state_apply(&mino_deform, shader);
            shader_set_int(shader, "u_has_ao",
                           voxel_mesh_has_ao(&ps->minotaur_parts.body) ? 1 : 0);
            shader_set_float(shader, "u_ao_intensity", 1.0f);
            voxel_mesh_draw(&ps->minotaur_parts.body);

            /* ── Draw horns (with retraction during roll) ── */
            shader_set_float(shader, "u_deform_height", 0.0f);
            shader_set_float(shader, "u_actor_height", 0.0f);  /* disable world-space AO for horns */
            {
                DeformState rigid_deform;
                deform_state_identity(&rigid_deform);
                deform_state_apply(&rigid_deform, shader);
            }
            if (ps->minotaur_parts.has_horns) {
                float horn_scale = 1.0f;
                #define HORN_RETRACT_DURATION 0.06f
                #define HORN_EXTEND_DURATION  0.08f
                if (in_roll) {
                    /* Horn retraction: animate at the start of the first real
                     * movement step, then stay hidden for subsequent steps.
                     *
                     * Forward:  STEP1 → STEP2.  First real step is always STEP1.
                     * Reverse:  STEP2 → STEP1.  First real step is STEP2 (2-step)
                     *           or STEP1 (1-step, since STEP2 is a placeholder).
                     *
                     * Placeholder steps (actually_moving=false) that precede the
                     * first real movement must keep horns visible to avoid flicker.
                     */
                    bool reversing = anim_queue_is_reversing(&ps->anim);
                    AnimPhase cur_phase = anim_queue_phase(&ps->anim);

                    if (actually_moving) {
                        /* Determine if this is the first real movement step */
                        bool is_first_movement;
                        if (!reversing) {
                            is_first_movement = (cur_phase == ANIM_PHASE_MINOTAUR_STEP1);
                        } else {
                            /* In reverse, first real step is STEP2 if 2-step,
                             * or STEP1 if 1-step (STEP2 was placeholder). */
                            if (ps->anim.record.minotaur_steps >= 2) {
                                is_first_movement = (cur_phase == ANIM_PHASE_MINOTAUR_STEP2);
                            } else {
                                is_first_movement = (cur_phase == ANIM_PHASE_MINOTAUR_STEP1);
                            }
                        }

                        if (is_first_movement && roll_t < HORN_RETRACT_DURATION) {
                            /* Scale Y from 1→0 with ease-in */
                            float t = roll_t / HORN_RETRACT_DURATION;
                            horn_scale = 1.0f - t * t;
                        } else {
                            horn_scale = 0.0f;
                        }
                    } else {
                        /* Placeholder step with no actual movement.
                         * Forward: placeholder is step2 (after retraction) → hidden.
                         * Reverse: placeholder is step2 (before real step1) → visible. */
                        horn_scale = reversing ? 1.0f : 0.0f;
                    }
                } else if (ps->mino_wobble_active &&
                           ps->mino_wobble_timer < HORN_EXTEND_DURATION) {
                    /* Animated horn extension: scale Y from 0→1 with ease-out
                     * over the first 0.08s of the post-roll wobble. */
                    float t = ps->mino_wobble_timer / HORN_EXTEND_DURATION;
                    float s = 1.0f - (1.0f - t) * (1.0f - t);
                    horn_scale = s;
                }

                if (horn_scale > 0.01f) {
                    /* Apply Y-scale for retraction on top of the body model matrix */
                    float horn_model[16], sy_mat[16], tmp[16];
                    mat4_scale_y(sy_mat, horn_scale);
                    mat4_mul(horn_model, model, sy_mat);
                    shader_set_mat4(shader, "u_model", horn_model);
                    shader_set_int(shader, "u_has_ao", 0);  /* no AO — clean bright horns */
                    voxel_mesh_draw(&ps->minotaur_parts.horns);
                }
            }

        }

        /* Theseus — hops during move animation */
        {
            float tcol, trow;
            float thop = 0.0f;
            if (animating &&
                anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
                anim_queue_theseus_event_type(&ps->anim) == ANIM_EVT_BOX_SLIDE) {
                /* Multi-phase push rendering:
                 * The tweens are linear 0→1. We remap progress to create:
                 * Phase 1 (0.00–0.20): approach — Theseus moves to edge near box
                 * Phase 2 (0.20–0.75): push — Theseus pressed against box, both
                 *   travel together one full tile. Theseus ends up past his
                 *   tile center, flush against the box at its destination.
                 * Phase 3 (0.75–1.00): settle — Theseus backs up to tile center
                 */
                float t = tween_progress(&ps->anim.move_x);
                float from_col = ps->anim.move_x.start;
                float from_row = ps->anim.move_y.start;
                float to_col   = ps->anim.move_x.end;
                float to_row   = ps->anim.move_y.end;
                float dir_col  = to_col - from_col; /* +1, -1, or 0 */
                float dir_row  = to_row - from_row;

                /* Contact offset: how far past tile center Theseus is
                 * when pressed against the box (edge of actor to edge of box) */
                float contact = 0.4f;

                if (t < 0.20f) {
                    /* Approach: from center to contact edge */
                    float u = t / 0.20f;
                    float s = u * u * (3.0f - 2.0f * u); /* smoothstep */
                    tcol = from_col + dir_col * contact * s;
                    trow = from_row + dir_row * contact * s;
                } else if (t < 0.75f) {
                    /* Push: Theseus stays pressed against box, both slide
                     * one full tile together.
                     * At t=0.20: Theseus at from + contact*dir (pressed against box at box_from)
                     * At t=0.75: Theseus at to + contact*dir (pressed against box at box_to) */
                    float u = (t - 0.20f) / 0.55f;
                    float s = u * u * (3.0f - 2.0f * u); /* smoothstep */
                    float push_start_col = from_col + dir_col * contact;
                    float push_start_row = from_row + dir_row * contact;
                    float push_end_col   = to_col + dir_col * contact;
                    float push_end_row   = to_row + dir_row * contact;
                    tcol = push_start_col + (push_end_col - push_start_col) * s;
                    trow = push_start_row + (push_end_row - push_start_row) * s;
                } else {
                    /* Settle: Theseus backs up from box to center of new tile */
                    float u = (t - 0.75f) / 0.25f;
                    float s = u * u * (3.0f - 2.0f * u); /* smoothstep */
                    float settle_start_col = to_col + dir_col * contact;
                    float settle_start_row = to_row + dir_row * contact;
                    tcol = settle_start_col + (to_col - settle_start_col) * s;
                    trow = settle_start_row + (to_row - settle_start_row) * s;
                }
                thop = 0.0f;
            } else if (animating) {
                anim_queue_theseus_pos(&ps->anim, &tcol, &trow, &thop);
            } else {
                tcol = (float)ps->grid->theseus_col;
                trow = (float)ps->grid->theseus_row;
            }

            /* Apply bump offset for failed push animation.
             * Ease profile: approach (0→0.30), linger (0.30→0.60), return (0.60→1.0).
             * Max displacement = 0.35 tiles toward the box. */
            if (ps->bump_active) {
                float t = ps->bump_timer;
                float bump_frac;
                if (t < 0.30f) {
                    /* Approach: smoothstep to contact */
                    float u = t / 0.30f;
                    bump_frac = u * u * (3.0f - 2.0f * u);
                } else if (t < 0.60f) {
                    /* Linger at contact — slight press-in then ease back slightly
                     * to simulate straining against the immovable box */
                    float u = (t - 0.30f) / 0.30f;
                    /* Small oscillation: press 5% further in, then back to 1.0 */
                    bump_frac = 1.0f + 0.05f * sinf(u * (float)M_PI);
                } else {
                    /* Give up and return */
                    float u = (t - 0.60f) / 0.40f;
                    bump_frac = 1.0f - u * u * (3.0f - 2.0f * u);
                }
                float max_disp = 0.35f;
                tcol += ps->bump_dir_x * bump_frac * max_disp;
                trow += ps->bump_dir_z * bump_frac * max_disp;
            }

            /* Ice wall bump: positional offset driven by anim_queue (Step 6.6).
             * The wall bump sub-phase provides a 0→1 progress value.
             * Phase 1 (0→contact_frac): slide toward wall at constant velocity.
             * Phase 2 (contact_frac→1): damped elastic bounce back to center.
             * Max displacement: Theseus's leading edge touches the wall's inner face. */
            #define ICE_BUMP_CONTACT_FRAC  (0.06f / ANIM_ICE_WALL_BUMP_DUR)
            #define ICE_BUMP_MAX_DISP  (0.5f - WALL_THICKNESS_3D * 0.5f - THESEUS_BODY_SIZE * 0.5f)
            {
                float bump_dir_x, bump_dir_z;
                float bump_t = anim_queue_ice_bump_progress(&ps->anim,
                                                             &bump_dir_x, &bump_dir_z);
                if (bump_t >= 0.0f) {
                    float disp;
                    if (bump_t < ICE_BUMP_CONTACT_FRAC) {
                        /* Slide toward wall at constant velocity */
                        disp = (bump_t / ICE_BUMP_CONTACT_FRAC) * ICE_BUMP_MAX_DISP;
                    } else {
                        /* Bounce back: damped elastic from max displacement to 0 */
                        float u = (bump_t - ICE_BUMP_CONTACT_FRAC) /
                                  (1.0f - ICE_BUMP_CONTACT_FRAC);
                        float bounce = cosf(u * 12.0f) * expf(-u * 5.0f);
                        if (bounce < 0.0f) bounce *= 0.3f; /* limit backward overshoot */
                        disp = ICE_BUMP_MAX_DISP * bounce;
                    }
                    tcol += bump_dir_x * disp;
                    trow += bump_dir_z * disp;
                }
            }

            /* Compute groove Y offset for Theseus (conveyor elevation + groove trench) */
            float thes_gy = actor_groove_y(ps, tcol, trow)
                          + actor_conveyor_y(ps, tcol, trow);
            float hop_y = thop * 0.3f;

            /* Compute turnstile platform rotation for Theseus (shared by
             * shadow + body).  Same 85/15 easing as platform mesh. */
            float thes_turnstile_yaw = 0.0f;
            if (animating && anim_queue_phase(&ps->anim) == ANIM_PHASE_ENVIRONMENT) {
                const AnimEvent* env_cur = anim_queue_current_event(&ps->anim);
                if (env_cur && env_cur->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE &&
                    env_cur->turnstile.actor_moved[0]) {
                    float raw_t = anim_queue_rotation_progress(&ps->anim);
                    bool cw_dir = env_cur->turnstile.clockwise;
                    float target = cw_dir ? (float)M_PI_2 : -(float)M_PI_2;
                    if (anim_queue_is_reversing(&ps->anim)) {
                        thes_turnstile_yaw = target * (raw_t - 1.0f);
                    } else if (raw_t < 0.85f) {
                        thes_turnstile_yaw = target * (raw_t / 0.85f);
                    } else {
                        float u = (raw_t - 0.85f) / 0.15f;
                        float osc = sinf(u * (float)M_PI * 2.0f)
                                  * (1.0f - u) * 0.04f;
                        thes_turnstile_yaw = target * (1.0f + osc);
                    }
                }
            }

            /* Soft ground shadow — shrinks with hop height.
             * Always at Y=0.01 (floor level) regardless of trench.
             * GL_LESS passes on rim, trench floor, and normal floor,
             * but correctly fails against groove box top (Y=0.45). */
            float shadow_scale = 1.0f - thop * 0.5f;
            draw_actor_shadow_multiplane_rot(ps, shader,
                tcol + 0.5f, trow + 0.5f, shadow_scale,
                thes_turnstile_yaw,
                ps->shadow_tex_theseus, ps->shadow_extent_t);

            /* ── Compute Theseus deformation ── */
            DeformState deform;
            deform_state_identity(&deform);

            /* Check animation sub-types for deformation routing */
            bool in_push = animating &&
                           anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
                           anim_queue_theseus_event_type(&ps->anim) == ANIM_EVT_BOX_SLIDE;
            bool in_turnstile = animating &&
                                anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
                                anim_queue_theseus_event_type(&ps->anim) == ANIM_EVT_TURNSTILE_ROTATE;
            bool in_hop = animating &&
                          anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
                          !in_push && !in_turnstile &&
                          !anim_queue_is_ice_sliding(&ps->anim) &&
                          !anim_queue_is_ice_wall_bumping(&ps->anim);

            if (in_push) {
                /* ── Groove box push deformation (Step 6.6) ──
                 * Directional squish along push axis during push phase.
                 * Phase 1 (0.00–0.20): approach — squish ramps up
                 * Phase 2 (0.20–0.75): push — sustained squish + slight lean
                 * Phase 3 (0.75–1.00): settle — squish releases */
                float t = tween_progress(&ps->anim.move_x);
                float dir_col = ps->anim.move_x.end - ps->anim.move_x.start;
                float dir_row = ps->anim.move_y.end - ps->anim.move_y.start;

                deform.squish_dir_x = dir_col;
                deform.squish_dir_z = dir_row;

                if (t < 0.20f) {
                    /* Approach: ramp squish from 0 to 0.12 */
                    float u = t / 0.20f;
                    float s = u * u * (3.0f - 2.0f * u);
                    deform.squish_amount = 0.12f * s;
                    deform.lean_x = dir_col * 0.03f * s;
                    deform.lean_z = dir_row * 0.03f * s;
                } else if (t < 0.75f) {
                    /* Push: sustained squish, slight lean forward */
                    deform.squish_amount = 0.12f;
                    deform.lean_x = dir_col * 0.03f;
                    deform.lean_z = dir_row * 0.03f;
                    /* Subtle squash during strain */
                    deform.squash = 0.97f;
                    deform.flare = 0.02f;
                } else {
                    /* Settle: squish releases with slight overshoot */
                    float u = (t - 0.75f) / 0.25f;
                    float s = u * u * (3.0f - 2.0f * u);
                    deform.squish_amount = 0.12f * (1.0f - s);
                    deform.lean_x = dir_col * 0.03f * (1.0f - s);
                    deform.lean_z = dir_row * 0.03f * (1.0f - s);
                    deform.squash = 0.97f + 0.03f * s;
                }
            } else if (in_turnstile) {
                /* ── Turnstile push deformation (Step 6.6) ──
                 * Lighter squish than groove box (0.10).
                 * Direction: Theseus pushes toward the junction. */
                float t = tween_progress(&ps->anim.move_x);
                const TurnRecord* rec = &ps->anim.record;
                float tdir_x = 0.0f, tdir_z = 0.0f;
                for (int ei = 0; ei < rec->event_count; ei++) {
                    if (rec->events[ei].type == ANIM_EVT_TURNSTILE_ROTATE) {
                        /* Push direction: toward junction from Theseus start */
                        tdir_x = (float)(rec->events[ei].turnstile.junction_col -
                                         rec->events[ei].from_col);
                        tdir_z = (float)(rec->events[ei].turnstile.junction_row -
                                         rec->events[ei].from_row);
                        /* Normalize (should already be ±1 or ±1,±1) */
                        float len = sqrtf(tdir_x * tdir_x + tdir_z * tdir_z);
                        if (len > 0.01f) { tdir_x /= len; tdir_z /= len; }
                        break;
                    }
                }
                deform.squish_dir_x = tdir_x;
                deform.squish_dir_z = tdir_z;

                /* Ease squish in and out over the turnstile duration */
                if (t < 0.30f) {
                    float u = t / 0.30f;
                    float s = u * u * (3.0f - 2.0f * u);
                    deform.squish_amount = 0.10f * s;
                    deform.lean_x = tdir_x * 0.02f * s;
                    deform.lean_z = tdir_z * 0.02f * s;
                } else if (t < 0.70f) {
                    deform.squish_amount = 0.10f;
                    deform.lean_x = tdir_x * 0.02f;
                    deform.lean_z = tdir_z * 0.02f;
                } else {
                    float u = (t - 0.70f) / 0.30f;
                    float s = u * u * (3.0f - 2.0f * u);
                    deform.squish_amount = 0.10f * (1.0f - s);
                    deform.lean_x = tdir_x * 0.02f * (1.0f - s);
                    deform.lean_z = tdir_z * 0.02f * (1.0f - s);
                }
            } else if (in_hop) {
                float t = tween_progress(&ps->anim.move_x);
                /* Movement direction for lean */
                float dc = ps->hop_dir_col;
                float dr = ps->hop_dir_row;

                /* Anticipation squat (t = 0.0 – 0.1) */
                if (t < 0.10f) {
                    float u = t / 0.10f;
                    float s = u * u * (3.0f - 2.0f * u); /* smoothstep */
                    deform.squash = 1.0f - 0.08f * s;    /* → 0.92 */
                    deform.flare  = 0.06f * s;
                }
                /* Jump elongation (t = 0.1 – 0.5) */
                else if (t < 0.50f) {
                    float u = (t - 0.10f) / 0.40f;
                    float s = u * u * (3.0f - 2.0f * u);
                    /* Transition from squat (0.92) to stretch (1.06) */
                    deform.squash = 0.92f + 0.14f * s;
                    deform.flare  = 0.06f * (1.0f - s);
                }
                /* Airborne narrowing (t = 0.5 – 0.85) */
                else if (t < 0.85f) {
                    float u = (t - 0.50f) / 0.35f;
                    /* Taper from 1.06 back toward 1.0 */
                    deform.squash = 1.06f - 0.06f * u;
                }
                /* Landing flare (t = 0.85 – 1.0) */
                else {
                    float u = (t - 0.85f) / 0.15f;
                    float s = u * u * (3.0f - 2.0f * u);
                    deform.squash = 1.0f - 0.10f * s;    /* → 0.90 */
                    deform.flare  = 0.12f * s;
                }

                /* Lean: shear in movement direction */
                if (t < 0.10f) {
                    /* No lean during squat */
                } else if (t < 0.50f) {
                    /* Lean forward: ramp up from 0 to 0.06 */
                    float u = (t - 0.10f) / 0.40f;
                    float lean_mag = 0.06f * sinf(u * (float)M_PI * 0.5f);
                    deform.lean_x = dc * lean_mag;
                    deform.lean_z = dr * lean_mag;
                } else if (t < 0.90f) {
                    /* Transition from forward lean to backward lean */
                    float u = (t - 0.50f) / 0.40f;
                    float lean_mag = 0.06f * (1.0f - u) - 0.03f * u;
                    deform.lean_x = dc * lean_mag;
                    deform.lean_z = dr * lean_mag;
                } else {
                    /* Approaching landing: slight backward lean fading out */
                    float u = (t - 0.90f) / 0.10f;
                    float lean_mag = -0.03f * (1.0f - u);
                    deform.lean_x = dc * lean_mag;
                    deform.lean_z = dr * lean_mag;
                }
            }
            /* Post-hop damped wobble — subtle settle, not jelly */
            else if (ps->wobble_active) {
                float wt = ps->wobble_timer;
                float amplitude = 0.04f;
                float freq = 22.0f;
                float damping = 18.0f;
                float wobble = amplitude * sinf(wt * freq) * expf(-wt * damping);
                deform.squash = 1.0f + wobble;
                deform.flare  = (1.0f - deform.squash) * 0.3f;
                if (deform.flare < 0.0f) deform.flare = 0.0f;
            }

            /* ── Failed push (bump) deformation (Step 6.6) ──
             * Layered on top of the positional bump offset above.
             * Theseus compresses against the immovable box, then bounces back. */
            if (ps->bump_active) {
                float t = ps->bump_timer;
                deform.squish_dir_x = ps->bump_dir_x;
                deform.squish_dir_z = ps->bump_dir_z;

                if (t < 0.30f) {
                    /* Approach: ramp squish to 0.18 (harder than successful push) */
                    float u = t / 0.30f;
                    float s = u * u * (3.0f - 2.0f * u);
                    deform.squish_amount = 0.18f * s;
                    deform.lean_x = ps->bump_dir_x * 0.04f * s;
                    deform.lean_z = ps->bump_dir_z * 0.04f * s;
                    deform.squash = 1.0f - 0.04f * s;
                    deform.flare  = 0.03f * s;
                } else if (t < 0.60f) {
                    /* Linger: subtle squish oscillation at contact */
                    float u = (t - 0.30f) / 0.30f;
                    float osc = sinf(u * (float)M_PI * 2.0f);
                    deform.squish_amount = 0.18f + 0.03f * osc;
                    deform.lean_x = ps->bump_dir_x * 0.04f;
                    deform.lean_z = ps->bump_dir_z * 0.04f;
                    deform.squash = 0.96f;
                    deform.flare  = 0.03f;
                } else {
                    /* Return: elastic bounce-back with overshoot */
                    float u = (t - 0.60f) / 0.40f;
                    float s = u * u * (3.0f - 2.0f * u);
                    /* Overshoot: squish goes negative briefly (expansion) */
                    float release = 0.18f * (1.0f - s) - 0.05f * sinf(s * (float)M_PI);
                    deform.squish_amount = release;
                    deform.lean_x = ps->bump_dir_x * 0.04f * (1.0f - s);
                    deform.lean_z = ps->bump_dir_z * 0.04f * (1.0f - s);
                    deform.squash = 0.96f + 0.04f * s;
                    deform.flare  = 0.03f * (1.0f - s);
                }
            }

            /* ── Ice wall bump deformation (Step 6.6) ──
             * Driven by anim_queue sub-phase (supports forward + reverse).
             * Approach: lean forward. Impact: strong directional squish + squash.
             * Bounce: damped elastic recovery. */
            {
                float bump_dir_x2, bump_dir_z2;
                float bump_p = anim_queue_ice_bump_progress(&ps->anim,
                                                             &bump_dir_x2, &bump_dir_z2);
                if (bump_p >= 0.0f && !ps->bump_active) {
                    deform.squish_dir_x = bump_dir_x2;
                    deform.squish_dir_z = bump_dir_z2;

                    if (bump_p < ICE_BUMP_CONTACT_FRAC) {
                        /* Approaching wall: forward lean builds */
                        float u = bump_p / ICE_BUMP_CONTACT_FRAC;
                        deform.lean_x = bump_dir_x2 * 0.05f * u;
                        deform.lean_z = bump_dir_z2 * 0.05f * u;
                    } else {
                        /* Post-impact: damped squish oscillation */
                        float u = (bump_p - ICE_BUMP_CONTACT_FRAC) /
                                  (1.0f - ICE_BUMP_CONTACT_FRAC);
                        float osc = 0.22f * cosf(u * 20.0f) * expf(-u * 6.0f);
                        deform.squish_amount = osc;
                        float squash_osc = fabsf(osc);
                        deform.squash = 1.0f - squash_osc * 0.4f;
                        deform.flare  = squash_osc * 0.25f;
                        float lean_osc = 0.06f * cosf(u * 14.0f) * expf(-u * 5.0f);
                        deform.lean_x = -bump_dir_x2 * lean_osc;
                        deform.lean_z = -bump_dir_z2 * lean_osc;
                    }
                }
            }

            /* ── Post-push/turnstile squish recovery (Step 6.6) ──
             * Damped elastic oscillation in squish direction.
             * Uses cosine so the effect starts at peak on impact, then rings down. */
            if (ps->squish_recovery_active && !ps->bump_active
                && !anim_queue_is_ice_wall_bumping(&ps->anim)) {
                #define SQUISH_RECOVERY_DURATION 0.25f
                #define SQUISH_RECOVERY_FREQ    22.0f
                #define SQUISH_RECOVERY_DAMPING 10.0f
                float wt = ps->squish_recovery_timer;
                float amp = ps->squish_recovery_amplitude;
                float osc = amp * cosf(wt * SQUISH_RECOVERY_FREQ)
                          * expf(-wt * SQUISH_RECOVERY_DAMPING);
                deform.squish_dir_x = ps->squish_recovery_dir_x;
                deform.squish_dir_z = ps->squish_recovery_dir_z;
                deform.squish_amount = osc;
                /* Add slight squash for extra juiciness on strong impacts */
                if (fabsf(amp) > 0.10f) {
                    deform.squash = 1.0f - fabsf(osc) * 0.3f;
                    deform.flare  = fabsf(osc) * 0.2f;
                }
            }

            /* Actor with AO fading based on hop.
             * Negative height = deform positions only, skip normal correction
             * (avoids Mach banding at subdivision boundaries). */
            float ao_intensity = 1.0f - thop;
            shader_set_float(shader, "u_deform_height",
                             -ps->theseus_parts.body_height);
            /* World-space actor AO for Theseus */
            shader_set_float(shader, "u_actor_ground_y", thes_gy);
            shader_set_float(shader, "u_actor_height",
                             ps->theseus_parts.body_height);
            deform_state_apply(&deform, shader);
            shader_set_int(shader, "u_has_ao",
                           voxel_mesh_has_ao(&ps->theseus_parts.body) ? 1 : 0);
            shader_set_float(shader, "u_ao_intensity", ao_intensity);

            float model[16];
            if (fabsf(thes_turnstile_yaw) > 0.001f) {
                /* Rotating with turnstile platform */
                float t_pos[16], ry_mat[16];
                mat4_translate(t_pos, tcol + 0.5f, thes_gy + hop_y, trow + 0.5f);
                mat4_rot_y(ry_mat, thes_turnstile_yaw);
                mat4_mul(model, t_pos, ry_mat);
            } else {
                memset(model, 0, sizeof(model));
                model[0]  = 1.0f;
                model[5]  = 1.0f;
                model[10] = 1.0f;
                model[15] = 1.0f;
                model[12] = tcol + 0.5f;
                model[13] = thes_gy + hop_y;
                model[14] = trow + 0.5f;
            }
            shader_set_mat4(shader, "u_model", model);
            voxel_mesh_draw(&ps->theseus_parts.body);
        }

        /* Blend stays enabled (engine default) */

        /* Reset uniforms for any subsequent draws */
        shader_set_float(shader, "u_deform_height", 0.0f);
        shader_set_float(shader, "u_actor_height", 0.0f);
        {
            DeformState identity_deform;
            deform_state_identity(&identity_deform);
            deform_state_apply(&identity_deform, shader);
        }
        shader_set_float(shader, "u_ao_intensity", 1.0f);
        shader_set_int(shader, "u_has_ao", 0);
        float identity2[16];
        memset(identity2, 0, sizeof(identity2));
        identity2[0] = identity2[5] = identity2[10] = identity2[15] = 1.0f;
        shader_set_mat4(shader, "u_model", identity2);
    }

    /* Render dust puff particles (after all opaque geometry, with depth read) */
    dust_puff_render(diorama_camera_get_vp(&ps->diorama_cam),
                     diorama_camera_get_view(&ps->diorama_cam));

    /* Restore camera target after shake offset */
    ps->diorama_cam.target[0] = saved_target[0];
    ps->diorama_cam.target[1] = saved_target[1];
    ps->diorama_cam.target[2] = saved_target[2];

    /* Restore state for 2D UI rendering */
    glDisable(GL_DEPTH_TEST);
}

/* ---------- State callbacks ---------- */

static void puzzle_on_enter(State* self) {
    PuzzleScene* ps = (PuzzleScene*)self;

    /* Load level */
    ps->grid = level_load_from_file(ps->level_path);
    if (!ps->grid) {
        LOG_ERROR("puzzle_scene: failed to load level '%s'", ps->level_path);
        set_status(ps, "Failed to load level!", COLOR_LOSS, 5.0f);
        return;
    }

    /* Initialize undo */
    undo_init(&ps->undo);
    undo_save_initial(&ps->undo, ps->grid);

    /* Reset state */
    ps->show_result = false;
    ps->result_is_win = false;
    ps->last_vw = 0;
    ps->last_vh = 0;
    ps->anim_result_pending = false;
    ps->undo_anim_pending = false;

    /* Initialize animation system */
    anim_queue_init(&ps->anim);
    input_buffer_init(&ps->input_buf);
    ps->bump_active = false;
    ps->bump_timer = 0.0f;
    ps->wobble_active = false;
    ps->wobble_timer = 0.0f;
    ps->was_theseus_hopping = false;
    ps->hop_dir_col = 0.0f;
    ps->hop_dir_row = 0.0f;
    ps->mino_wobble_active = false;
    ps->mino_wobble_timer = 0.0f;
    ps->was_mino_rolling = false;
    ps->mino_facing_angle = 0.0f;
    ps->was_pushing = false;
    ps->was_turnstile_pushing = false;
    ps->push_dir_x = 0.0f;
    ps->push_dir_z = 0.0f;
    ps->squish_recovery_active = false;
    ps->squish_recovery_timer = 0.0f;
    ps->debug_anim_speed = 1.0f;

    /* Build 3D diorama for verification */
    ps->render_3d = false;
    ps->diorama_built = false;
    build_diorama(ps);

    input_manager_set_context(INPUT_CONTEXT_PUZZLE);

    LOG_INFO("Puzzle scene: loaded '%s' (%s) — %dx%d grid",
             ps->grid->level_id, ps->grid->level_name,
             ps->grid->cols, ps->grid->rows);
}

static void puzzle_on_exit(State* self) {
    PuzzleScene* ps = (PuzzleScene*)self;
    undo_clear(&ps->undo);
    if (ps->diorama_built) {
        voxel_mesh_destroy(&ps->diorama_mesh);
        destroy_turnstile_meshes(ps);
        actor_render_destroy(&ps->theseus_parts);
        actor_render_destroy(&ps->minotaur_parts);
        voxel_mesh_destroy(&ps->groove_box_mesh);
        destroy_shadow_resources(ps);
        free(ps->groove_tile_map);
        ps->groove_tile_map = NULL;
        free(ps->conveyor_tile_map);
        ps->conveyor_tile_map = NULL;
        free(ps->turnstile_tile_map);
        ps->turnstile_tile_map = NULL;
        ps->diorama_built = false;
    }
    if (ps->grid) {
        grid_destroy(ps->grid);
        ps->grid = NULL;
    }
}

static void puzzle_on_resume(State* self) {
    input_manager_set_context(INPUT_CONTEXT_PUZZLE);
    (void)self;
}

static void puzzle_handle_action(State* self, SemanticAction action) {
    PuzzleScene* ps = (PuzzleScene*)self;

    /* Pause/back always works immediately, even during animation */
    if (action == ACTION_PAUSE) {
        engine_pop_state();
        return;
    }

    /* Debug: 'C' toggles between 2D and 3D rendering */
    if (action == ACTION_DEBUG_TOGGLE_CAMERA) {
        ps->render_3d = !ps->render_3d;
        LOG_INFO("Render mode: %s", ps->render_3d ? "3D diorama" : "2D flat");
        return;
    }

    /* Debug: 'V' toggles between orthographic and perspective projection */
    if (action == ACTION_DEBUG_TOGGLE_PROJECTION) {
        g_settings.camera_perspective = !g_settings.camera_perspective;
        LOG_INFO("Projection: %s (FOV %.0f°)",
                 g_settings.camera_perspective ? "perspective" : "orthographic",
                 g_settings.camera_fov);
        return;
    }

    /* Debug: 'B' toggles cel-shaded rendering */
    if (action == ACTION_DEBUG_TOGGLE_CEL) {
        g_settings.cel_shading = !g_settings.cel_shading;
        LOG_INFO("Cel-shading: %s", g_settings.cel_shading ? "ON" : "OFF");
        return;
    }

    /* Debug: 'I'/'K' adjust camera pitch */
    if (action == ACTION_DEBUG_PITCH_UP) {
        ps->diorama_cam.pitch = CLAMP(ps->diorama_cam.pitch + 5.0f, 5.0f, 85.0f);
        LOG_INFO("Camera pitch: %.0f°", ps->diorama_cam.pitch);
        return;
    }
    if (action == ACTION_DEBUG_PITCH_DOWN) {
        ps->diorama_cam.pitch = CLAMP(ps->diorama_cam.pitch - 5.0f, 5.0f, 85.0f);
        LOG_INFO("Camera pitch: %.0f°", ps->diorama_cam.pitch);
        return;
    }

    /* Debug: 'O'/'L' adjust FOV */
    if (action == ACTION_DEBUG_FOV_UP) {
        g_settings.camera_fov = CLAMP(g_settings.camera_fov + 1.0f, 5.0f, 90.0f);
        LOG_INFO("FOV: %.0f°", g_settings.camera_fov);
        return;
    }
    if (action == ACTION_DEBUG_FOV_DOWN) {
        g_settings.camera_fov = CLAMP(g_settings.camera_fov - 1.0f, 5.0f, 90.0f);
        LOG_INFO("FOV: %.0f°", g_settings.camera_fov);
        return;
    }
    if (action == ACTION_DEBUG_ANIM_SPEED) {
        /* Cycle: 1.0 → 0.5 → 0.25 → 0.125 → 1.0 */
        if (ps->debug_anim_speed > 0.9f)       ps->debug_anim_speed = 0.5f;
        else if (ps->debug_anim_speed > 0.4f)  ps->debug_anim_speed = 0.25f;
        else if (ps->debug_anim_speed > 0.2f)  ps->debug_anim_speed = 0.125f;
        else                                     ps->debug_anim_speed = 1.0f;
        LOG_INFO("Animation speed: %.3fx", ps->debug_anim_speed);
        return;
    }

    if (!ps->grid) return;

    /* During animation: buffer eligible actions during minotaur's last step */
    if (anim_queue_is_playing(&ps->anim)) {
        if (anim_queue_in_buffer_window(&ps->anim)) {
            input_buffer_accept(&ps->input_buf, action);
        }
        /* During death animation, always accept undo/reset */
        if (ps->anim_result_pending &&
            (action == ACTION_UNDO || action == ACTION_RESET)) {
            input_buffer_accept(&ps->input_buf, action);
        }
        return;
    }

    /* Not animating — process action normally */
    switch (action) {
        case ACTION_MOVE_NORTH:
        case ACTION_MOVE_SOUTH:
        case ACTION_MOVE_EAST:
        case ACTION_MOVE_WEST:
        case ACTION_WAIT:
        case ACTION_UNDO:
        case ACTION_RESET:
            resolve_action(ps, action);
            break;

        default:
            break;
    }
}

static void puzzle_update(State* self, float dt) {
    PuzzleScene* ps = (PuzzleScene*)self;

    if (ps->status_timer > 0.0f) {
        ps->status_timer -= dt;
    }

    /* Update bump animation (failed push against groove box) */
    if (ps->bump_active) {
        #define BUMP_DURATION 0.40f
        ps->bump_timer += dt / BUMP_DURATION;
        if (ps->bump_timer >= 1.0f) {
            ps->bump_timer = 1.0f;
            ps->bump_active = false;
        }
    }

    /* Update post-hop wobble timer */
    if (ps->wobble_active) {
        #define WOBBLE_MAX_DURATION 0.18f
        ps->wobble_timer += dt;
        if (ps->wobble_timer >= WOBBLE_MAX_DURATION) {
            ps->wobble_active = false;
        }
    }

    /* Update animation — must run BEFORE wobble detection so that
     * phase transitions are visible in the same frame, avoiding a
     * one-frame gap where horns flash to full scale. */
    if (anim_queue_is_playing(&ps->anim)) {
        /* Open/close buffer window */
        if (anim_queue_in_buffer_window(&ps->anim) &&
            !input_buffer_window_is_open(&ps->input_buf)) {
            input_buffer_open_window(&ps->input_buf);
        }

        /* Fast-forward animations when input is buffered or a key is held.
         * Held keys don't generate repeat KEY_DOWN events (filtered by input_manager),
         * so we also check SDL_GetKeyboardState to detect held direction/wait keys.
         * This way, holding a direction key speeds up the current animation. */
        bool has_pending_input = (ps->input_buf.buffered != ACTION_NONE) ||
                                 (input_buffer_check_held_keys() != ACTION_NONE);
        anim_queue_set_fast_forward(&ps->anim, has_pending_input);

        anim_queue_update(&ps->anim, dt * ps->debug_anim_speed);

        /* Check if animation just completed */
        if (!anim_queue_is_playing(&ps->anim)) {
            input_buffer_close_window(&ps->input_buf);

            /* Complete deferred undo (grid restore after reverse animation) */
            if (ps->undo_anim_pending) {
                ps->undo_anim_pending = false;
                undo_pop(&ps->undo, ps->grid);

                /* Regenerate all turnstile meshes from the now-correct
                 * grid state.  Without this, meshes built during the
                 * reverse animation still reflect the POST-turn wall
                 * layout, causing a visible flicker on the next forward
                 * turn. */
                for (int ti = 0; ti < ps->turnstile_mesh_count; ti++) {
                    if (ps->turnstile_meshes[ti].valid)
                        regenerate_turnstile_mesh(ps, ti);
                    ps->turnstile_meshes[ti].held_angle = 0.0f;
                }

                /* Check for buffered action (e.g. rapid undo presses) */
                SemanticAction buffered = input_buffer_consume(&ps->input_buf);
                if (buffered == ACTION_NONE) {
                    buffered = input_buffer_check_held_keys();
                }
                if (buffered != ACTION_NONE) {
                    resolve_action(ps, buffered);
                }
                return;
            }

            /* Show deferred result */
            if (ps->anim_result_pending) {
                show_turn_result(ps, ps->pending_result);
                ps->anim_result_pending = false;
            }

            /* Check for buffered action */
            SemanticAction buffered = input_buffer_consume(&ps->input_buf);
            if (buffered == ACTION_NONE) {
                /* Check held keys per §10.4 item 2 */
                buffered = input_buffer_check_held_keys();
            }

            if (buffered != ACTION_NONE) {
                resolve_action(ps, buffered);
            }
        }
    }

    /* Detect hop-end transition: was hopping last frame, not anymore → start wobble.
     * Runs AFTER anim_queue_update so phase transitions are seen immediately. */
    {
        bool is_hopping = anim_queue_is_playing(&ps->anim) &&
                          anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
                          anim_queue_theseus_event_type(&ps->anim) != ANIM_EVT_BOX_SLIDE &&
                          anim_queue_theseus_event_type(&ps->anim) != ANIM_EVT_TURNSTILE_ROTATE &&
                          !anim_queue_is_ice_sliding(&ps->anim) &&
                          !anim_queue_is_ice_wall_bumping(&ps->anim);
        if (ps->was_theseus_hopping && !is_hopping) {
            ps->wobble_active = true;
            ps->wobble_timer = 0.0f;
        }
        /* Track direction while hopping */
        if (is_hopping) {
            float from_col = ps->anim.move_x.start;
            float from_row = ps->anim.move_y.start;
            float to_col   = ps->anim.move_x.end;
            float to_row   = ps->anim.move_y.end;
            float dc = to_col - from_col;
            float dr = to_row - from_row;
            if (dc != 0.0f || dr != 0.0f) {
                ps->hop_dir_col = dc;
                ps->hop_dir_row = dr;
            }
        }
        ps->was_theseus_hopping = is_hopping;
    }

    /* Detect push-end transition → start elastic squish recovery (Step 6.6).
     * Triggers when Theseus was in a BOX_SLIDE phase and just left it. */
    {
        bool is_pushing = anim_queue_is_playing(&ps->anim) &&
                          anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
                          anim_queue_theseus_event_type(&ps->anim) == ANIM_EVT_BOX_SLIDE;
        if (is_pushing) {
            /* Track push direction for recovery */
            ps->push_dir_x = ps->anim.move_x.end - ps->anim.move_x.start;
            ps->push_dir_z = ps->anim.move_y.end - ps->anim.move_y.start;
        }
        if (ps->was_pushing && !is_pushing) {
            ps->squish_recovery_active = true;
            ps->squish_recovery_timer = 0.0f;
            ps->squish_recovery_amplitude = -0.04f; /* negative = expand (overshoot) */
            ps->squish_recovery_dir_x = ps->push_dir_x;
            ps->squish_recovery_dir_z = ps->push_dir_z;
        }
        ps->was_pushing = is_pushing;
    }

    /* Detect turnstile push end → start elastic squish recovery (Step 6.6). */
    {
        bool is_turnstile = anim_queue_is_playing(&ps->anim) &&
                            anim_queue_phase(&ps->anim) == ANIM_PHASE_THESEUS &&
                            anim_queue_theseus_event_type(&ps->anim) == ANIM_EVT_TURNSTILE_ROTATE;
        if (is_turnstile) {
            /* Extract push direction from event for recovery */
            const TurnRecord* rec = &ps->anim.record;
            for (int ei = 0; ei < rec->event_count; ei++) {
                if (rec->events[ei].type == ANIM_EVT_TURNSTILE_ROTATE) {
                    float dx = (float)(rec->events[ei].turnstile.junction_col -
                                       rec->events[ei].from_col);
                    float dz = (float)(rec->events[ei].turnstile.junction_row -
                                       rec->events[ei].from_row);
                    float len = sqrtf(dx * dx + dz * dz);
                    if (len > 0.01f) { dx /= len; dz /= len; }
                    ps->push_dir_x = dx;
                    ps->push_dir_z = dz;
                    break;
                }
            }
        }
        if (ps->was_turnstile_pushing && !is_turnstile) {
            ps->squish_recovery_active = true;
            ps->squish_recovery_timer = 0.0f;
            ps->squish_recovery_amplitude = -0.03f; /* lighter than groove box */
            ps->squish_recovery_dir_x = ps->push_dir_x;
            ps->squish_recovery_dir_z = ps->push_dir_z;
        }
        ps->was_turnstile_pushing = is_turnstile;
    }

    /* Update squish recovery timer (Step 6.6) */
    if (ps->squish_recovery_active) {
        ps->squish_recovery_timer += dt;
        if (ps->squish_recovery_timer >= SQUISH_RECOVERY_DURATION) {
            ps->squish_recovery_active = false;
        }
    }

    /* Auto-turnstile environment animation end → regenerate wall meshes.
     * Per-turnstile tracking: each turnstile regenerates as soon as its
     * specific animation event finishes, avoiding a one-frame gap when
     * multiple turnstiles animate sequentially. */
    for (int ti = 0; ti < ps->turnstile_mesh_count; ti++) {
        bool is_animating = false;
        if (anim_queue_is_playing(&ps->anim) &&
            anim_queue_phase(&ps->anim) == ANIM_PHASE_ENVIRONMENT) {
            const AnimEvent* cur = anim_queue_current_event(&ps->anim);
            if (cur && cur->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE &&
                cur->turnstile.junction_col == ps->turnstile_meshes[ti].jc &&
                cur->turnstile.junction_row == ps->turnstile_meshes[ti].jr) {
                is_animating = true;
            }
        }
        if (ps->turnstile_meshes[ti].was_animating && !is_animating) {
            /* During reverse (undo) playback, the turnstile event finishes
             * early (events play in reverse order) but undo_pop hasn't
             * restored the grid yet.  Skip regeneration here — the meshes
             * will be rebuilt from the correct grid state after undo_pop
             * completes (see the undo_anim_pending block above). */
            if (anim_queue_is_reversing(&ps->anim)) {
                /* Latch the final reverse rotation angle so the mesh
                 * stays in the correct visual position while the rest
                 * of the undo animation (theseus move, etc.) plays out.
                 * Cleared when undo_pop regenerates meshes. */
                float target = ps->turnstile_meshes[ti].clockwise
                    ? (float)M_PI_2 : -(float)M_PI_2;
                ps->turnstile_meshes[ti].held_angle = -target;
                ps->turnstile_meshes[ti].was_animating = is_animating;
                continue;
            }

            /* Update minotaur facing angle if it was on this turnstile */
            if (anim_queue_is_playing(&ps->anim) || ps->anim.record.event_count > 0) {
                /* Find the most recent auto-turnstile event for this junction */
                for (int ei = 0; ei < ps->anim.record.event_count; ei++) {
                    const AnimEvent* e = &ps->anim.record.events[ei];
                    if (e->type == ANIM_EVT_AUTO_TURNSTILE_ROTATE &&
                        e->turnstile.junction_col == ps->turnstile_meshes[ti].jc &&
                        e->turnstile.junction_row == ps->turnstile_meshes[ti].jr &&
                        e->turnstile.actor_moved[1]) {
                        /* Minotaur was on this turnstile — rotate facing angle.
                         * Sign matches platform rendering: cw → +π/2. */
                        float rot = ps->turnstile_meshes[ti].clockwise
                            ? (float)M_PI_2 : -(float)M_PI_2;
                        ps->mino_facing_angle += rot;
                        break;
                    }
                }
            }
            regenerate_turnstile_mesh(ps, ti);
        }
        ps->turnstile_meshes[ti].was_animating = is_animating;
    }

    /* Minotaur roll-end detection → start landing wobble.
     * Only triggers when the minotaur actually moved (minotaur_steps > 0).
     * Zero-step turns run placeholder tweens through the minotaur phases
     * but shouldn't trigger wobble or horn extension animations.
     * Runs AFTER anim_queue_update so the transition is detected on the
     * same frame the phase changes, avoiding a one-frame horn flash. */
    {
        bool is_rolling = anim_queue_is_playing(&ps->anim) &&
                          (anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP1 ||
                           anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP2) &&
                          ps->anim.record.minotaur_steps > 0;
        bool is_reversing = anim_queue_is_reversing(&ps->anim);

        if (ps->was_mino_rolling && !is_rolling) {
            ps->mino_wobble_active = true;
            ps->mino_wobble_timer = 0.0f;

            /* Update minotaur facing direction based on last movement.
             * 0 = facing -Z (north), π/2 = facing +X (east), etc. */
            {
                float dx = ps->anim.mino_x.end - ps->anim.mino_x.start;
                float dz = ps->anim.mino_y.end - ps->anim.mino_y.start;
                if (fabsf(dx) > 0.01f || fabsf(dz) > 0.01f) {
                    ps->mino_facing_angle = atan2f(dx, -dz);
                }
            }

            /* Trigger stomp effects (dust puffs + camera shake).
             * Skip during undo — effects are too hard to get right in reverse.
             * Use the animation tween's end position rather than grid state,
             * because grid state is already updated for the NEXT move when
             * input is buffered, but the minotaur is still finishing this one. */
            if (!is_reversing) {
                float mcol = ps->anim.mino_x.end;
                float mrow = ps->anim.mino_y.end;
                dust_puff_spawn(mcol + 0.5f, 0.0f, mrow + 0.5f);

                /* Start camera shake */
                #define SHAKE_DURATION 0.15f
                #define SHAKE_AMPLITUDE 0.03f
                ps->shake_timer = SHAKE_DURATION;
            }
        }

        /* Detect step1→step2 transition for mid-roll stomp on two-step moves.
         * When the minotaur takes two steps, we also spawn dust at the
         * intermediate landing between step1 and step2 (forward only). */
        if (is_rolling && !is_reversing) {
            bool in_step1 = anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP1;
            bool in_step2 = anim_queue_phase(&ps->anim) == ANIM_PHASE_MINOTAUR_STEP2;
            if (ps->was_mino_in_step1 && in_step2 &&
                ps->anim.record.minotaur_steps >= 2) {
                /* Minotaur just landed from first step — spawn dust at
                 * the end position of step1 (= start of step2). */
                float mid_col = ps->anim.mino_x.start;
                float mid_row = ps->anim.mino_y.start;
                dust_puff_spawn(mid_col + 0.5f, 0.0f, mid_row + 0.5f);

                #define SHAKE_MID_DURATION 0.10f
                ps->shake_timer = SHAKE_MID_DURATION;
            }
            ps->was_mino_in_step1 = in_step1;
        } else {
            ps->was_mino_in_step1 = false;
        }

        ps->was_mino_rolling = is_rolling;
    }

    /* Update minotaur post-roll wobble timer */
    if (ps->mino_wobble_active) {
        #define MINO_WOBBLE_MAX 0.20f
        ps->mino_wobble_timer += dt;
        if (ps->mino_wobble_timer >= MINO_WOBBLE_MAX) {
            ps->mino_wobble_active = false;
        }
    }

    /* Update camera shake — smooth damped sine oscillation.
     * Uses two slightly different frequencies on X and Z for organic feel. */
    #define SHAKE_FREQ_X   18.0f  /* radians/sec */
    #define SHAKE_FREQ_Z   22.0f  /* radians/sec (slightly different for Lissajous wobble) */
    #define SHAKE_DAMPING   8.0f  /* exponential decay rate */
    if (ps->shake_timer > 0.0f) {
        ps->shake_timer -= dt;
        if (ps->shake_timer <= 0.0f) {
            ps->shake_timer = 0.0f;
            ps->shake_offset_x = 0.0f;
            ps->shake_offset_z = 0.0f;
        } else {
            float intensity = g_settings.shake_intensity;
            float elapsed = SHAKE_DURATION - ps->shake_timer;
            float envelope = expf(-elapsed * SHAKE_DAMPING);
            float amp = SHAKE_AMPLITUDE * intensity * envelope;
            ps->shake_offset_x = amp * sinf(elapsed * SHAKE_FREQ_X);
            ps->shake_offset_z = amp * sinf(elapsed * SHAKE_FREQ_Z);
        }
    }

    /* Update dust puff particles */
    dust_puff_update(dt);
}

static void puzzle_render(State* self) {
    PuzzleScene* ps = (PuzzleScene*)self;
    if (!ps->grid) {
        renderer_clear(COLOR_BG);
        return;
    }

    int vw, vh;
    renderer_get_viewport(&vw, &vh);

    /* Recalculate layout if viewport changed */
    if (vw != ps->last_vw || vh != ps->last_vh) {
        calc_layout(ps, vw, vh);
    }

    renderer_clear(COLOR_BG);

    /* Render 3D diorama when 'C' toggle is active */
    if (ps->render_3d) {
        if (g_settings.cel_shading) {
            /* Render to FBO for outline post-processing */
            renderer_begin_outline_pass(vw, vh);
            render_diorama(ps, vw, vh);
            renderer_end_outline_pass(vw, vh);
        } else {
            render_diorama(ps, vw, vh);
        }
    }

    /* Draw 2D layers when not in 3D mode */
    if (!ps->render_3d) {
        render_floor(ps);
        render_doors(ps);
        render_features(ps);
        render_walls(ps);
        render_actors(ps);
    }

    /* Rewind overlay during reverse (undo) animation */
    if (anim_queue_is_reversing(&ps->anim) && anim_queue_is_playing(&ps->anim)) {
        /* Semi-transparent blue-tinted overlay for "rewind tape" effect */
        ui_draw_rect(0, 0, (float)vw, (float)vh,
                     color_rgba(0.05f, 0.08f, 0.15f, 0.25f));

        /* Horizontal scan lines for VHS-rewind feel */
        float line_h = 2.0f;
        float gap = 12.0f;
        Color line_color = color_rgba(0.2f, 0.3f, 0.5f, 0.08f);
        for (float y = 0; y < (float)vh; y += gap) {
            ui_draw_rect(0, y, (float)vw, line_h, line_color);
        }
    }

    render_hud(ps, vw, vh);
    render_debug_labels(ps, vw, vh);
    render_result_overlay(ps, vw, vh);
}

static void puzzle_destroy(State* self) {
    PuzzleScene* ps = (PuzzleScene*)self;
    undo_clear(&ps->undo);
    if (ps->diorama_built) {
        dust_puff_shutdown();
        voxel_mesh_destroy(&ps->diorama_mesh);
        destroy_turnstile_meshes(ps);
        actor_render_destroy(&ps->theseus_parts);
        actor_render_destroy(&ps->minotaur_parts);
        voxel_mesh_destroy(&ps->groove_box_mesh);
        destroy_shadow_resources(ps);
        free(ps->groove_tile_map);
        ps->groove_tile_map = NULL;
        free(ps->conveyor_tile_map);
        ps->conveyor_tile_map = NULL;
        free(ps->turnstile_tile_map);
        ps->turnstile_tile_map = NULL;
    }
    if (ps->grid) {
        grid_destroy(ps->grid);
    }
    free(ps);
}

/* ---------- Create ---------- */

State* puzzle_scene_create(const char* level_json_path) {
    PuzzleScene* ps = (PuzzleScene*)calloc(1, sizeof(PuzzleScene));
    if (!ps) return NULL;

    snprintf(ps->level_path, sizeof(ps->level_path), "%s", level_json_path);

    ps->base.on_enter      = puzzle_on_enter;
    ps->base.on_exit       = puzzle_on_exit;
    ps->base.on_pause      = NULL;
    ps->base.on_resume     = puzzle_on_resume;
    ps->base.handle_action = puzzle_handle_action;
    ps->base.update        = puzzle_update;
    ps->base.render        = puzzle_render;
    ps->base.destroy       = puzzle_destroy;
    ps->base.transparent   = false;

    return (State*)ps;
}
