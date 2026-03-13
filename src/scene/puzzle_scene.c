#include "scene/puzzle_scene.h"
#include "engine/engine.h"
#include "engine/utils.h"
#include "render/renderer.h"
#include "render/ui_draw.h"
#include "render/text_render.h"
#include "input/input_manager.h"
#include "data/strings.h"
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

/* Convert grid coordinates to screen pixel position (top-left of tile) */
static void grid_to_screen(const PuzzleScene* ps, int col, int row,
                           float* out_x, float* out_y) {
    /* col increases left-to-right */
    *out_x = ps->grid_origin_x + col * ps->tile_size;
    /* row increases S→N, but screen Y increases downward.
     * Row (rows-1) should be at grid_origin_y (top).
     * Row 0 should be at grid_origin_y + (rows-1)*tile_size (bottom). */
    *out_y = ps->grid_origin_y + (ps->grid->rows - 1 - row) * ps->tile_size;
}

/* ---------- Set status message ---------- */

static void set_status(PuzzleScene* ps, const char* text, Color color, float duration) {
    snprintf(ps->status_text, sizeof(ps->status_text), "%s", text);
    ps->status_color = color;
    ps->status_timer = duration;
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

static void resolve_action(PuzzleScene* ps, SemanticAction action) {
    if (ps->show_result) {
        /* In result state, only undo/reset/back allowed */
        if (action == ACTION_UNDO) {
            if (undo_pop(&ps->undo, ps->grid)) {
                ps->show_result = false;
                set_status(ps, "Undo", COLOR_HUD, 0.8f);
            }
            return;
        }
        if (action == ACTION_RESET) {
            if (undo_reset(&ps->undo, ps->grid)) {
                ps->show_result = false;
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
        case ACTION_UNDO:
            if (undo_pop(&ps->undo, ps->grid)) {
                set_status(ps, "Undo", COLOR_HUD, 0.8f);
            }
            return;
        case ACTION_RESET:
            if (undo_reset(&ps->undo, ps->grid)) {
                set_status(ps, "Reset", COLOR_HUD, 0.8f);
            }
            return;
        default:
            return;
    }

    /* Push undo snapshot BEFORE resolving */
    undo_push(&ps->undo, ps->grid);

    /* Resolve the turn */
    TurnResult result;
    if (is_wait) {
        result = turn_resolve(ps->grid, DIR_NONE);
    } else {
        result = turn_resolve(ps->grid, dir);
    }

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
            /* Move was blocked, undo the snapshot we just pushed */
            undo_pop(&ps->undo, ps->grid);
            break;

        case TURN_RESULT_CONTINUE:
            /* Normal turn completed */
            break;
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
            bool locked = f->vt->snapshot_size && f->vt->snapshot_size(f) > 0;
            /* Check locked state by reading data directly */
            typedef struct { int gate_side; bool locked; } LGPeek;
            LGPeek* lgd = (LGPeek*)f->data;
            Color gc = lgd->locked ? COLOR_GATE_LOCKED : COLOR_GATE_OPEN;
            float gi = ts * 0.30f;
            ui_draw_rect(sx + gi, sy + gi, ts - 2.0f * gi, ts - 2.0f * gi, gc);
        } else if (strcmp(name, "teleporter") == 0) {
            /* Diamond shape (rotated square) approximated with small centered square */
            float ti = ts * 0.25f;
            ui_draw_rect(sx + ti, sy + ti, ts - 2.0f * ti, ts - 2.0f * ti, COLOR_TELEPORTER);
        } else if (strcmp(name, "crumbling_floor") == 0) {
            bool collapsed = f->vt->is_hazardous &&
                             f->vt->is_hazardous(f, ps->grid, f->col, f->row);
            Color cc = collapsed ? COLOR_CRUMBLE_PIT : COLOR_CRUMBLE_OK;
            float ci = ts * 0.10f;
            ui_draw_rect(sx + ci, sy + ci, ts - 2.0f * ci, ts - 2.0f * ci, cc);
            /* Crack pattern for intact */
            if (!collapsed) {
                float line_w = 2.0f;
                ui_draw_rect(sx + ts * 0.3f, sy + ts * 0.2f, line_w, ts * 0.6f, COLOR_BG);
                ui_draw_rect(sx + ts * 0.2f, sy + ts * 0.5f, ts * 0.5f, line_w, COLOR_BG);
            }
        } else if (strcmp(name, "moving_platform") == 0) {
            /* Draw platform at its current position */
            float pi = ts * 0.08f;
            ui_draw_rect(sx + pi, sy + pi, ts - 2.0f * pi, ts - 2.0f * pi, COLOR_PLATFORM);
        } else if (strcmp(name, "medusa_wall") == 0) {
            /* Green eye marker */
            float ei = ts * 0.30f;
            ui_draw_rect(sx + ei, sy + ei, ts - 2.0f * ei, ts - 2.0f * ei, COLOR_MEDUSA);
        } else if (strcmp(name, "ice_tile") == 0) {
            /* Semi-transparent blue overlay */
            ui_draw_rect(sx, sy, ts, ts, COLOR_ICE);
        } else if (strcmp(name, "groove_box") == 0) {
            /* Feature col/row tracks the box position, so sx/sy is correct */
            float bi = ts * 0.12f;
            ui_draw_rect(sx + bi, sy + bi, ts - 2.0f * bi, ts - 2.0f * bi,
                         COLOR_GROOVE_BOX);
        } else if (strcmp(name, "auto_turnstile") == 0 ||
                   strcmp(name, "manual_turnstile") == 0) {
            /* Small gear/pivot marker */
            float gi = ts * 0.35f;
            ui_draw_rect(sx + gi, sy + gi, ts - 2.0f * gi, ts - 2.0f * gi,
                         COLOR_TURNSTILE);
        } else if (strcmp(name, "conveyor") == 0) {
            /* Colored tile with directional arrow using small rects */
            float ci = ts * 0.05f;
            ui_draw_rect(sx + ci, sy + ci, ts - 2.0f * ci, ts - 2.0f * ci,
                         COLOR_CONVEYOR);

            /* Peek at direction to draw arrow indicator */
            typedef struct { Direction dir; } ConvPeek;
            ConvPeek* cd = (ConvPeek*)f->data;
            float cx = sx + ts * 0.5f;
            float cy = sy + ts * 0.5f;
            float aw = ts * 0.08f;  /* arrow element width */
            float al = ts * 0.25f;  /* arrow shaft length */
            Color ac = {0.25f, 0.35f, 0.20f, 1.0f};

            /* Shaft */
            if (cd->dir == DIR_EAST || cd->dir == DIR_WEST) {
                ui_draw_rect(cx - al, cy - aw * 0.5f, al * 2.0f, aw, ac);
            } else {
                ui_draw_rect(cx - aw * 0.5f, cy - al, aw, al * 2.0f, ac);
            }

            /* Arrowhead (small square at the leading edge) */
            float ah = ts * 0.12f;
            float hx = cx, hy = cy;
            if (cd->dir == DIR_EAST)  hx = cx + al;
            if (cd->dir == DIR_WEST)  hx = cx - al;
            if (cd->dir == DIR_NORTH) hy = cy - al;  /* screen up = north */
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
        float inset = ts * EXIT_MARKER_INSET;
        ui_draw_rect(sx + inset, sy + inset,
                     ts - 2.0f * inset, ts - 2.0f * inset,
                     COLOR_EXIT);
    }

    /* Entrance tile marker */
    {
        float sx, sy;
        grid_to_screen(ps, ps->grid->entrance_col, ps->grid->entrance_row, &sx, &sy);
        float inset = ts * ENTRANCE_MARKER_INSET;
        ui_draw_rect(sx + inset, sy + inset,
                     ts - 2.0f * inset, ts - 2.0f * inset,
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
                /* Skip if this is the exit door */
                if (!(c == ps->grid->exit_col && r == ps->grid->exit_row &&
                      ps->grid->exit_side == DIR_NORTH)) {
                    ui_draw_rect(sx - wt * 0.5f, sy - wt * 0.5f,
                                 ts + wt, wt, COLOR_WALL);
                }
            }

            /* South wall (bottom edge in screen coords) */
            if (grid_has_wall(ps->grid, c, r, DIR_SOUTH)) {
                if (!(c == ps->grid->exit_col && r == ps->grid->exit_row &&
                      ps->grid->exit_side == DIR_SOUTH)) {
                    ui_draw_rect(sx - wt * 0.5f, sy + ts - wt * 0.5f,
                                 ts + wt, wt, COLOR_WALL);
                }
            }

            /* West wall (left edge) */
            if (grid_has_wall(ps->grid, c, r, DIR_WEST)) {
                if (!(c == ps->grid->exit_col && r == ps->grid->exit_row &&
                      ps->grid->exit_side == DIR_WEST)) {
                    ui_draw_rect(sx - wt * 0.5f, sy - wt * 0.5f,
                                 wt, ts + wt, COLOR_WALL);
                }
            }

            /* East wall (right edge) */
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

    /* Minotaur (drawn first so Theseus appears on top) */
    {
        float msize = ts * MINOTAUR_SIZE_FRAC;
        float sx, sy;
        grid_to_screen(ps, ps->grid->minotaur_col, ps->grid->minotaur_row, &sx, &sy);
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
        grid_to_screen(ps, ps->grid->theseus_col, ps->grid->theseus_row, &sx, &sy);
        float offset = (ts - tsize) * 0.5f;
        ui_draw_rect(sx + offset, sy + offset, tsize, tsize, COLOR_THESEUS);
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

    input_manager_set_context(INPUT_CONTEXT_PUZZLE);

    LOG_INFO("Puzzle scene: loaded '%s' (%s) — %dx%d grid",
             ps->grid->level_id, ps->grid->level_name,
             ps->grid->cols, ps->grid->rows);
}

static void puzzle_on_exit(State* self) {
    PuzzleScene* ps = (PuzzleScene*)self;
    undo_clear(&ps->undo);
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

    /* Pause/back always works, even if level failed to load */
    if (action == ACTION_PAUSE) {
        engine_pop_state();
        return;
    }

    if (!ps->grid) return;

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

    /* Draw layers bottom-up */
    render_floor(ps);
    render_doors(ps);
    render_features(ps);
    render_walls(ps);
    render_actors(ps);
    render_hud(ps, vw, vh);
    render_result_overlay(ps, vw, vh);
}

static void puzzle_destroy(State* self) {
    PuzzleScene* ps = (PuzzleScene*)self;
    undo_clear(&ps->undo);
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
