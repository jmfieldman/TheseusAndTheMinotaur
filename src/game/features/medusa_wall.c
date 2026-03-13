#include "medusa_wall.h"
#include "../grid.h"
#include "../../engine/utils.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Direction facing;       /* direction the Medusa faces (into the grid) */
    Direction wall_side;    /* which wall the Medusa is mounted on */
} MedusaWallData;

/* ── Helpers ──────────────────────────────────────────── */

static Direction parse_dir(const char* s) {
    if (!s) return DIR_NONE;
    if (strcmp(s, "north") == 0) return DIR_NORTH;
    if (strcmp(s, "south") == 0) return DIR_SOUTH;
    if (strcmp(s, "east") == 0)  return DIR_EAST;
    if (strcmp(s, "west") == 0)  return DIR_WEST;
    return DIR_NONE;
}

/*
 * Check if Theseus at (tc, tr) is in the Medusa's line of sight.
 * The Medusa is at (self->col, self->row) facing `facing`.
 * LOS extends in the facing direction from the Medusa's cell.
 */
static bool in_line_of_sight(const Feature* self, const Grid* grid,
                              Direction facing, int tc, int tr) {
    int dc = direction_dcol(facing);
    int dr = direction_drow(facing);

    /* Walk along the LOS from the Medusa's cell */
    int c = self->col;
    int r = self->row;

    /* The Medusa sits on a wall, so LOS starts from the cell in front.
     * Actually, the Medusa is at (col, row) and faces into the grid.
     * LOS includes (col, row) itself and extends outward.
     * But the Medusa is ON the wall, so it shouldn't include its own cell
     * if the actor is standing there... Actually per design, the Medusa
     * is on a wall segment. Let's start LOS from the Medusa's cell
     * and extend in the facing direction. */

    while (true) {
        /* Check if target is at this cell */
        if (c == tc && r == tr) return true;

        /* Check for wall blocking further LOS */
        if (grid_has_wall(grid, c, r, facing)) return false;

        /* Advance */
        c += dc;
        r += dr;

        /* Out of bounds? */
        if (!grid_in_bounds(grid, c, r)) return false;
    }
}

/* ── Vtable hooks ─────────────────────────────────────── */

static PreMoveResult medusa_on_pre_move(const Feature* self, const Grid* grid,
                                         int from_col, int from_row,
                                         int to_col, int to_row,
                                         Direction dir) {
    (void)to_col; (void)to_row;
    const MedusaWallData* d = (const MedusaWallData*)self->data;

    /* Only kills Theseus moving TOWARD the Medusa.
     * "Toward" means the player's move direction is the OPPOSITE of the
     * Medusa's facing direction. */
    if (dir != direction_opposite(d->facing)) return PREMOVE_OK;

    /* Check if Theseus (at from_col, from_row) is in the Medusa's LOS */
    if (in_line_of_sight(self, grid, d->facing, from_col, from_row)) {
        return PREMOVE_KILL;
    }

    return PREMOVE_OK;
}

static void medusa_destroy(Feature* self) {
    free(self->data);
    self->data = NULL;
}

static const FeatureVTable medusa_wall_vt = {
    .name                  = "medusa_wall",
    .blocks_movement       = NULL,
    .on_pre_move           = medusa_on_pre_move,
    .on_enter              = NULL,
    .on_leave              = NULL,
    .on_push               = NULL,
    .on_environment_phase  = NULL,
    .is_hazardous          = NULL,
    .snapshot_size          = NULL,
    .snapshot_save          = NULL,
    .snapshot_restore       = NULL,
    .destroy               = medusa_destroy,
};

/* ── Factory ──────────────────────────────────────────── */

Feature* medusa_wall_create(int col, int row, const cJSON* config) {
    Feature* f = feature_create(&medusa_wall_vt, col, row);
    if (!f) return NULL;

    MedusaWallData* d = calloc(1, sizeof(MedusaWallData));
    if (!d) { feature_free(f); return NULL; }

    d->facing = DIR_SOUTH;
    d->wall_side = DIR_NORTH;

    if (config) {
        const cJSON* facing_item = cJSON_GetObjectItemCaseSensitive(config, "facing");
        if (cJSON_IsString(facing_item)) {
            d->facing = parse_dir(facing_item->valuestring);
        }

        const cJSON* side_item = cJSON_GetObjectItemCaseSensitive(config, "side");
        if (cJSON_IsString(side_item)) {
            d->wall_side = parse_dir(side_item->valuestring);
        }
    }

    f->data = d;
    return f;
}
