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
#include "data/biome_config.h"
#include "input/input_manager.h"
#include "platform/platform.h"
#include "data/strings.h"
#include "data/settings.h"
#include "game/game.h"

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
    VoxelMesh       theseus_mesh;    /* dynamic actor: Theseus (blue cube) */
    VoxelMesh       minotaur_mesh;   /* dynamic actor: Minotaur (red cube) */
    VoxelMesh       groove_box_mesh; /* dynamic: groove box (wooden crate) */
    GLuint          shadow_vao;
    GLuint          shadow_vbo;      /* simple quad for actor shadow */
    int             shadow_vertex_count;
    GLuint          shadow_tex_theseus;  /* R8 blurred rectangular shadow texture */
    GLuint          shadow_tex_minotaur;
    float           shadow_extent_t;     /* world-space half-extent of Theseus shadow quad */
    float           shadow_extent_m;     /* world-space half-extent of Minotaur shadow quad */
    float           shadow_offset_x;     /* world-space shadow offset (simulates light angle) */
    float           shadow_offset_z;
    DioramaCamera   diorama_cam;
    LightingState   diorama_light;
    WallStyle       wall_style;      /* cached for shader uniforms at render time */

    /* Failed push "bump" animation (no game state change, purely visual) */
    bool            bump_active;
    float           bump_timer;      /* 0→1 progress */
    float           bump_dir_x;      /* direction of bump (+1/-1/0) */
    float           bump_dir_z;      /* direction of bump (+1/-1/0) */
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
    /* Minotaur: slightly shorter (size × size*0.8 × size) */
    generate_actor_shadow_texture(MINOTAUR_SIZE_FRAC, MINOTAUR_SIZE_FRAC * 0.8f, cfg,
                                   &ps->shadow_tex_minotaur, &ps->shadow_extent_m);

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
    ps->shadow_vertex_count = 0;
}

static void draw_shadow(const PuzzleScene* ps, GLuint shader,
                         float x, float z, float scale,
                         GLuint shadow_tex, float extent) {
    if (ps->shadow_vertex_count == 0 || !shadow_tex) return;

    /* Bind actor shadow texture to unit 1 (temporarily replaces floor lightmap) */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadow_tex);
    glActiveTexture(GL_TEXTURE0);

    /* Disable depth writes so shadow doesn't block actor rendering */
    glDepthMask(GL_FALSE);

    /* Quad spans [-1,+1] in local space; scale to shadow extent */
    float s = extent * scale;
    float model[16];
    memset(model, 0, sizeof(model));
    model[0]  = s;
    model[5]  = 1.0f;
    model[10] = s;
    model[15] = 1.0f;
    model[12] = x + ps->shadow_offset_x;
    model[13] = 0.01f;  /* just above floor */
    model[14] = z + ps->shadow_offset_z;
    shader_set_mat4(shader, "u_model", model);

    glBindVertexArray(ps->shadow_vao);
    glDrawArrays(GL_TRIANGLES, 0, ps->shadow_vertex_count);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);

    /* Restore floor lightmap on unit 1 */
    if (ps->diorama_mesh.floor_lm_texture) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ps->diorama_mesh.floor_lm_texture);
        glActiveTexture(GL_TEXTURE0);
    }
}

static void build_diorama(PuzzleScene* ps) {
    if (ps->diorama_built) {
        voxel_mesh_destroy(&ps->diorama_mesh);
        voxel_mesh_destroy(&ps->theseus_mesh);
        voxel_mesh_destroy(&ps->minotaur_mesh);
        voxel_mesh_destroy(&ps->groove_box_mesh);
        destroy_shadow_resources(ps);
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

    /* Generate diorama via procedural pipeline */
    voxel_mesh_begin(&ps->diorama_mesh);

    DioramaGenResult gen_result;
    diorama_generate(&ps->diorama_mesh, ps->grid, &biome, &gen_result);

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

    /* Build actor meshes (unit-sized, centered at origin).
     * Each actor includes an invisible ground plane occluder so the AO baker
     * sees a floor surface, producing darkened bottom edges on the cube. */

    /* Theseus — blue cube */
    {
        float size = 0.45f;
        float half = size * 0.5f;
        voxel_mesh_begin(&ps->theseus_mesh);
        voxel_mesh_add_box(&ps->theseus_mesh,
                            -half, 0.0f, -half,
                            size, size, size,
                            80.0f/255.0f, 168.0f/255.0f, 251.0f/255.0f, 1.0f,
                            true);
        voxel_mesh_add_occluder(&ps->theseus_mesh,
                                 -2.0f, -0.05f, -2.0f,
                                 4.0f, 0.05f, 4.0f);
        voxel_mesh_build(&ps->theseus_mesh, size * 0.25f);
    }

    /* Minotaur — red cube */
    {
        float size = 0.65f;
        float half = size * 0.5f;
        voxel_mesh_begin(&ps->minotaur_mesh);
        voxel_mesh_add_box(&ps->minotaur_mesh,
                            -half, 0.0f, -half,
                            size, size * 0.8f, size,
                            239.0f/255.0f, 34.0f/255.0f, 34.0f/255.0f, 1.0f,
                            true);
        voxel_mesh_add_occluder(&ps->minotaur_mesh,
                                 -2.0f, -0.05f, -2.0f,
                                 4.0f, 0.05f, 4.0f);
        voxel_mesh_build(&ps->minotaur_mesh, size * 0.25f);
    }

    /* Groove box — wooden crate */
    {
        /* Wall thickness matches diorama_gen.c WALL_THICKNESS (0.20).
         * Box fills tile minus wall inset on each side. */
        float inset = 0.12f;
        float box_sz = 1.0f - 2.0f * inset;
        float box_h  = 0.30f;
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

    LOG_INFO("Diorama built: %d static verts, theseus %d verts, minotaur %d verts",
             voxel_mesh_get_vertex_count(&ps->diorama_mesh),
             voxel_mesh_get_vertex_count(&ps->theseus_mesh),
             voxel_mesh_get_vertex_count(&ps->minotaur_mesh));
}

static void render_diorama(PuzzleScene* ps, int vw, int vh) {
    if (!ps->diorama_built) return;

    GLuint shader = renderer_get_voxel_shader();
    if (!shader) return;

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

    /* Draw static geometry (floor, walls, exit marker) */
    voxel_mesh_draw(&ps->diorama_mesh);

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

            float model[16];
            memset(model, 0, sizeof(model));
            model[0]  = 1.0f;
            model[5]  = 1.0f;
            model[10] = 1.0f;
            model[15] = 1.0f;
            model[12] = render_col + 0.5f;
            model[13] = 0.0f;
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

        /* Minotaur — always on ground (no hop) */
        {
            float mcol, mrow;
            if (animating) {
                anim_queue_minotaur_pos(&ps->anim, &mcol, &mrow);
            } else {
                mcol = (float)ps->grid->minotaur_col;
                mrow = (float)ps->grid->minotaur_row;
            }

            /* Soft ground shadow */
            draw_shadow(ps, shader, mcol + 0.5f, mrow + 0.5f, 1.0f,
                        ps->shadow_tex_minotaur, ps->shadow_extent_m);

            /* Actor with full AO */
            shader_set_int(shader, "u_has_ao",
                           voxel_mesh_has_ao(&ps->minotaur_mesh) ? 1 : 0);
            shader_set_float(shader, "u_ao_intensity", 1.0f);
            float model[16];
            memset(model, 0, sizeof(model));
            model[0]  = 1.0f;
            model[5]  = 1.0f;
            model[10] = 1.0f;
            model[15] = 1.0f;
            model[12] = mcol + 0.5f;
            model[13] = 0.0f;
            model[14] = mrow + 0.5f;
            shader_set_mat4(shader, "u_model", model);
            voxel_mesh_draw(&ps->minotaur_mesh);
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

            float hop_y = thop * 0.3f;

            /* Soft ground shadow — shrinks with hop height */
            float shadow_scale = 1.0f - thop * 0.5f;
            draw_shadow(ps, shader, tcol + 0.5f, trow + 0.5f, shadow_scale,
                        ps->shadow_tex_theseus, ps->shadow_extent_t);

            /* Actor with AO fading based on hop */
            float ao_intensity = 1.0f - thop;
            shader_set_int(shader, "u_has_ao",
                           voxel_mesh_has_ao(&ps->theseus_mesh) ? 1 : 0);
            shader_set_float(shader, "u_ao_intensity", ao_intensity);
            float model[16];
            memset(model, 0, sizeof(model));
            model[0]  = 1.0f;
            model[5]  = 1.0f;
            model[10] = 1.0f;
            model[15] = 1.0f;
            model[12] = tcol + 0.5f;
            model[13] = hop_y;
            model[14] = trow + 0.5f;
            shader_set_mat4(shader, "u_model", model);
            voxel_mesh_draw(&ps->theseus_mesh);
        }

        /* Blend stays enabled (engine default) */

        /* Reset uniforms for any subsequent draws */
        shader_set_float(shader, "u_ao_intensity", 1.0f);
        shader_set_int(shader, "u_has_ao", 0);
        float identity2[16];
        memset(identity2, 0, sizeof(identity2));
        identity2[0] = identity2[5] = identity2[10] = identity2[15] = 1.0f;
        shader_set_mat4(shader, "u_model", identity2);
    }

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
        voxel_mesh_destroy(&ps->theseus_mesh);
        voxel_mesh_destroy(&ps->minotaur_mesh);
        voxel_mesh_destroy(&ps->groove_box_mesh);
        destroy_shadow_resources(ps);
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

    /* Update animation */
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

        anim_queue_update(&ps->anim, dt);

        /* Check if animation just completed */
        if (!anim_queue_is_playing(&ps->anim)) {
            input_buffer_close_window(&ps->input_buf);

            /* Complete deferred undo (grid restore after reverse animation) */
            if (ps->undo_anim_pending) {
                ps->undo_anim_pending = false;
                undo_pop(&ps->undo, ps->grid);

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
        render_diorama(ps, vw, vh);
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
        voxel_mesh_destroy(&ps->diorama_mesh);
        voxel_mesh_destroy(&ps->theseus_mesh);
        voxel_mesh_destroy(&ps->minotaur_mesh);
        voxel_mesh_destroy(&ps->groove_box_mesh);
        destroy_shadow_resources(ps);
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
