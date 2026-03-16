#include "turn.h"
#include "minotaur.h"
#include "../engine/utils.h"
#include <string.h>

/* ── Environment phase ─────────────────────────────────── */

void turn_run_environment_phase(Grid* grid) {
    for (int i = 0; i < grid->feature_count; i++) {
        Feature* f = grid->features[i];
        if (f->vt->on_environment_phase) {
            f->vt->on_environment_phase(f, grid);
        }
    }
}

/* ── Pre-move check ────────────────────────────────────── */

/*
 * Run on_pre_move hooks for ALL features in the grid.
 * Any feature may respond (e.g., Medusa checks LOS from its wall).
 * Returns the most severe result (KILL > SLIDE > OK).
 */
static PreMoveResult run_pre_move_hooks(const Grid* grid,
                                         int from_col, int from_row,
                                         int to_col, int to_row,
                                         Direction dir) {
    PreMoveResult result = PREMOVE_OK;

    for (int i = 0; i < grid->feature_count; i++) {
        const Feature* f = grid->features[i];
        if (f->vt->on_pre_move) {
            PreMoveResult r = f->vt->on_pre_move(f, grid,
                                                   from_col, from_row,
                                                   to_col, to_row, dir);
            if (r == PREMOVE_KILL) return PREMOVE_KILL;
            if (r == PREMOVE_SLIDE) result = PREMOVE_SLIDE;
        }
    }

    return result;
}

/* ── Push check ────────────────────────────────────────── */

/*
 * When Theseus's move is blocked, check if any feature in the grid
 * has an on_push hook that consumes the action.
 *
 * We check ALL features (not just those at source/target) because some
 * features (e.g. manual turnstile) may respond to pushes from tiles
 * other than the one the feature is placed on.
 */
static bool try_push(Grid* grid, int from_col, int from_row, Direction dir) {
    for (int i = 0; i < grid->feature_count; i++) {
        Feature* f = grid->features[i];
        if (f->vt->on_push &&
            f->vt->on_push(f, grid, from_col, from_row, dir)) {
            return true;
        }
    }

    return false;
}

/* ── Ice slide ─────────────────────────────────────────── */

/*
 * Check if a tile has an ice feature.
 */
static bool tile_has_ice(const Grid* grid, int col, int row) {
    const Cell* cell = grid_cell_const(grid, col, row);
    if (!cell) return false;
    for (int i = 0; i < cell->feature_count; i++) {
        if (strcmp(cell->features[i]->vt->name, "ice_tile") == 0) {
            return true;
        }
    }
    return false;
}

/*
 * Slide Theseus across ice tiles in the given direction.
 * Called after Theseus has already moved onto an ice tile.
 * If ice_evt is non-NULL, records each slide position as a waypoint
 * for animation playback (ice_evt already has the first waypoint).
 */
static TurnResult ice_slide_with_waypoints(Grid* grid, Direction dir,
                                            AnimEvent* ice_evt) {
    while (tile_has_ice(grid, grid->theseus_col, grid->theseus_row)) {
        if (!grid_can_move(grid, ENTITY_THESEUS,
                           grid->theseus_col, grid->theseus_row, dir)) {
            break;
        }

        grid_move_entity(grid, ENTITY_THESEUS, dir);

        /* Record waypoint for animation */
        if (ice_evt &&
            ice_evt->ice_slide.waypoint_count < ICE_SLIDE_MAX_WAYPOINTS) {
            int idx = ice_evt->ice_slide.waypoint_count;
            ice_evt->ice_slide.waypoint_cols[idx] = grid->theseus_col;
            ice_evt->ice_slide.waypoint_rows[idx] = grid->theseus_row;
            ice_evt->ice_slide.waypoint_count++;
        }

        if (grid_entities_collide(grid)) {
            grid->level_lost = true;
            return TURN_RESULT_LOSS_COLLISION;
        }

        if (grid_theseus_on_hazard(grid)) {
            grid->level_lost = true;
            return TURN_RESULT_LOSS_HAZARD;
        }
    }

    return TURN_RESULT_CONTINUE;
}

/* ── Exit check ────────────────────────────────────────── */

static bool try_exit(Grid* grid, Direction player_dir) {
    if (grid->theseus_col == grid->exit_col &&
        grid->theseus_row == grid->exit_row &&
        player_dir == grid->exit_side) {
        grid->level_won = true;
        return true;
    }
    return false;
}

/* ── Helper: fill minotaur "no move" defaults in record ── */

static void record_minotaur_nomove(TurnRecord* record, const Grid* grid) {
    if (!record) return;
    record->minotaur_after1_col = grid->minotaur_col;
    record->minotaur_after1_row = grid->minotaur_row;
    record->minotaur_after2_col = grid->minotaur_col;
    record->minotaur_after2_row = grid->minotaur_row;
    record->minotaur_steps = 0;
}

/* ── Main turn resolution ──────────────────────────────── */

TurnResult turn_resolve(Grid* grid, Direction player_dir, TurnRecord* record) {
    TurnResult result;

    /* Initialize record if provided */
    if (record) {
        memset(record, 0, sizeof(*record));
        record->theseus_from_col   = grid->theseus_col;
        record->theseus_from_row   = grid->theseus_row;
        record->minotaur_start_col = grid->minotaur_col;
        record->minotaur_start_row = grid->minotaur_row;
    }

    /* Set active_record so features can push animation events */
    grid->active_record = record;

    /* ── Phase 1: Theseus ── */

    if (player_dir != DIR_NONE) {
        /* Check for exit first */
        if (try_exit(grid, player_dir)) {
            grid->turn_count++;
            if (record) {
                record->theseus_to_col = grid->theseus_col + direction_dcol(player_dir);
                record->theseus_to_row = grid->theseus_row + direction_drow(player_dir);
                record->theseus_moved  = true;
                record_minotaur_nomove(record, grid);
                record->result = TURN_RESULT_WIN;
            }
            result = TURN_RESULT_WIN;
            goto done;
        }

        int tc = grid->theseus_col + direction_dcol(player_dir);
        int tr = grid->theseus_row + direction_drow(player_dir);

        /* Run on_pre_move hooks before attempting the move */
        PreMoveResult pre = run_pre_move_hooks(grid,
                                                grid->theseus_col, grid->theseus_row,
                                                tc, tr, player_dir);
        if (pre == PREMOVE_KILL) {
            grid->level_lost = true;
            grid->turn_count++;
            if (record) {
                record->theseus_to_col = tc;
                record->theseus_to_row = tr;
                record->theseus_moved  = true;
                record_minotaur_nomove(record, grid);
                record->result = TURN_RESULT_LOSS_HAZARD;
            }
            result = TURN_RESULT_LOSS_HAZARD;
            goto done;
        }

        /* Try to move */
        if (!grid_move_entity(grid, ENTITY_THESEUS, player_dir)) {
            /* Move blocked — try on_push hooks */
            if (try_push(grid, grid->theseus_col, grid->theseus_row, player_dir)) {
                if (record) {
                    record->theseus_to_col = grid->theseus_col;
                    record->theseus_to_row = grid->theseus_row;
                    record->theseus_moved  = false;
                    record->theseus_pushed = true;
                }
            } else {
                if (record) {
                    record->theseus_to_col = grid->theseus_col;
                    record->theseus_to_row = grid->theseus_row;
                    record->theseus_moved  = false;
                    record->result = TURN_RESULT_BLOCKED;
                }
                result = TURN_RESULT_BLOCKED;
                goto done;
            }
        } else {
            /* Move succeeded — record normal hop event */
            if (record) {
                record->theseus_to_col = grid->theseus_col;
                record->theseus_to_row = grid->theseus_row;
                record->theseus_moved  = true;
            }

            if (pre == PREMOVE_SLIDE) {
                /* Record ice slide event with waypoints */
                AnimEvent ice_evt = {
                    .type  = ANIM_EVT_THESEUS_ICE_SLIDE,
                    .phase = ANIM_EVENT_PHASE_THESEUS,
                    .from_col = record ? record->theseus_from_col : grid->theseus_col,
                    .from_row = record ? record->theseus_from_row : grid->theseus_row,
                    .entity   = ENTITY_THESEUS,
                };
                /* First waypoint: the tile Theseus hopped to (first ice tile) */
                ice_evt.ice_slide.waypoint_cols[0] = grid->theseus_col;
                ice_evt.ice_slide.waypoint_rows[0] = grid->theseus_row;
                ice_evt.ice_slide.waypoint_count = 1;

                TurnResult slide_result = ice_slide_with_waypoints(grid, player_dir, &ice_evt);

                /* Final position after slide.
                 * If Theseus is still on ice, the slide ended by hitting a wall.
                 * If not on ice, he slid off onto a normal tile. */
                ice_evt.to_col = grid->theseus_col;
                ice_evt.to_row = grid->theseus_row;
                ice_evt.ice_slide.hit_wall = tile_has_ice(grid,
                                                           grid->theseus_col,
                                                           grid->theseus_row);
                turn_record_push_event(record, &ice_evt);

                if (slide_result != TURN_RESULT_CONTINUE) {
                    grid->turn_count++;
                    if (record) {
                        record->theseus_to_col = grid->theseus_col;
                        record->theseus_to_row = grid->theseus_row;
                        record_minotaur_nomove(record, grid);
                        record->result = slide_result;
                    }
                    result = slide_result;
                    goto done;
                }
                if (record) {
                    record->theseus_to_col = grid->theseus_col;
                    record->theseus_to_row = grid->theseus_row;
                }
            } else {
                /* Normal hop — push a hop event */
                AnimEvent hop_evt = {
                    .type     = ANIM_EVT_THESEUS_HOP,
                    .phase    = ANIM_EVENT_PHASE_THESEUS,
                    .from_col = record ? record->theseus_from_col : 0,
                    .from_row = record ? record->theseus_from_row : 0,
                    .to_col   = grid->theseus_col,
                    .to_row   = grid->theseus_row,
                    .entity   = ENTITY_THESEUS,
                };
                turn_record_push_event(record, &hop_evt);
            }

            if (grid_entities_collide(grid)) {
                grid->level_lost = true;
                grid->turn_count++;
                if (record) {
                    record_minotaur_nomove(record, grid);
                    record->result = TURN_RESULT_LOSS_COLLISION;
                }
                result = TURN_RESULT_LOSS_COLLISION;
                goto done;
            }

            if (grid_theseus_on_hazard(grid)) {
                grid->level_lost = true;
                grid->turn_count++;
                if (record) {
                    record_minotaur_nomove(record, grid);
                    record->result = TURN_RESULT_LOSS_HAZARD;
                }
                result = TURN_RESULT_LOSS_HAZARD;
                goto done;
            }
        }
    } else {
        /* Wait — Theseus stays put */
        if (record) {
            record->theseus_to_col = grid->theseus_col;
            record->theseus_to_row = grid->theseus_row;
            record->theseus_moved  = false;
        }
    }

    /* ── Phase 2: Environment ── */

    turn_run_environment_phase(grid);

    if (grid_theseus_on_hazard(grid)) {
        grid->level_lost = true;
        grid->turn_count++;
        if (record) {
            record_minotaur_nomove(record, grid);
            record->result = TURN_RESULT_LOSS_HAZARD;
        }
        result = TURN_RESULT_LOSS_HAZARD;
        goto done;
    }

    if (grid_entities_collide(grid)) {
        grid->level_lost = true;
        grid->turn_count++;
        if (record) {
            record_minotaur_nomove(record, grid);
            record->result = TURN_RESULT_LOSS_COLLISION;
        }
        result = TURN_RESULT_LOSS_COLLISION;
        goto done;
    }

    /* ── Phase 3: Minotaur (decomposed for animation recording) ── */

    int mino_steps = 0;

    /* Step 1 */
    if (minotaur_step(grid)) {
        mino_steps = 1;
    }

    if (record) {
        record->minotaur_after1_col = grid->minotaur_col;
        record->minotaur_after1_row = grid->minotaur_row;
    }

    if (grid_entities_collide(grid)) {
        grid->level_lost = true;
        grid->turn_count++;
        if (record) {
            record->minotaur_after2_col = grid->minotaur_col;
            record->minotaur_after2_row = grid->minotaur_row;
            record->minotaur_steps = mino_steps;
            record->result = TURN_RESULT_LOSS_COLLISION;
        }
        result = TURN_RESULT_LOSS_COLLISION;
        goto done;
    }

    /* Step 2 */
    if (minotaur_step(grid)) {
        mino_steps = 2;
    }

    if (record) {
        record->minotaur_after2_col = grid->minotaur_col;
        record->minotaur_after2_row = grid->minotaur_row;
        record->minotaur_steps = mino_steps;
    }

    if (grid_entities_collide(grid)) {
        grid->level_lost = true;
        grid->turn_count++;
        if (record) {
            record->result = TURN_RESULT_LOSS_COLLISION;
        }
        result = TURN_RESULT_LOSS_COLLISION;
        goto done;
    }

    grid->turn_count++;
    if (record) {
        record->result = TURN_RESULT_CONTINUE;
    }
    result = TURN_RESULT_CONTINUE;

done:
    grid->active_record = NULL;
    return result;
}
