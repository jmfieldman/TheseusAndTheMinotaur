# 13 -- Auto-Turnstile Visual Overhaul

## 1. Problem Statement

The auto-turnstile rotates 4 tiles worth of walls and actors 90° each
environment phase. Currently, the walls rotate visually but the **floor tiles
are static** and **actor movement is a linear slide** from source to
destination tile. This creates two readability problems:

1. **Unexplained actor movement.** When an actor stands on a turnstile tile
   but no wall touches it, the actor slides to a new tile with no visible
   cause. Players see an unexplained teleport.
2. **Linear slide doesn't match rotational motion.** Actors move in a straight
   line between tiles, but the mechanic is a 90° rotation around a junction
   point. The animation contradicts the underlying logic.

## 2. Design Goals

- Make it **instantly clear** that the 4 tiles are a single mechanical unit
  that rotates as a group.
- Give every actor movement a **visible physical cause** -- the platform they
  stand on is rotating, carrying them with it.
- Create a distinct **visual identity** that separates auto-turnstiles from
  conveyors (linear push) and manual turnstiles (walls only, no floor).
- Fit the **mechanical/industrial** aesthetic of the game's diorama style.

## 3. Visual Design

### 3.1 Raised Platform

The 4 tiles of an auto-turnstile form a **raised 2x2 platform**, elevated to
the same height as conveyor belts (`CONVEYOR_HEIGHT = 0.10`). This reuses the
established visual language: "raised platform = this tile affects your
movement."

- Platform top surface sits at Y = 0.10 (same as conveyor elevation).
- Platform sides are visible, matching the `platform_side` biome palette color.
- Yellow/black hazard stripes line the outer perimeter of the 2x2 block,
  using the same stripe rendering as conveyor platforms. Stripes appear only
  on edges that face **outward** (not on the internal seams between the 4
  tiles).

### 3.2 Rigidized Metal Surface

The top surface of the platform tiles uses a **rigidized metal** appearance --
a subtle raised diamond or linen pattern that reads as industrial steel plate.
This is visually distinct from:

- Normal floor tiles (stone/brick checkerboard)
- Conveyor belt surfaces (dark with animated slats)
- Manual turnstile tiles (normal floor, no visual distinction)

Implementation options (in order of preference):

1. **Procedural shader pattern:** A new `ao_mode` value (e.g.
   `AO_MODE_TURNSTILE_PLATE = 6`) triggers a fragment shader branch that
   overlays a subtle diamond-plate bump pattern on the base color. The pattern
   is static (does not scroll).
2. **Vertex color variation:** Bake a subtle checkerboard tint into the floor
   tile vertex colors during geometry generation. Simpler but less detailed.

Base color: metallic silver-grey, slightly brighter than walls. Suggested
RGB ~(0.52, 0.52, 0.56) with low color jitter.

### 3.3 Rotation Direction Indicator

A subtle visual cue on the platform surface indicates the rotation direction:

- **Curved arrow** etched/embossed into the center of the 2x2 platform,
  pointing CW or CCW. Implemented as a small arc of slightly darker or
  lighter voxel boxes placed during geometry generation.
- Alternatively: small triangular chevrons near each outer edge pointing in
  the rotation direction (similar to conveyor direction arrows but curved).

This is lower priority -- the rotation animation itself communicates direction
once seen. But a static indicator helps players plan before the first rotation.

### 3.4 Animated Gear Mechanism Underneath

Beneath the raised platform, visible in the gap between the platform edge and
the surrounding floor, sits **animated gear mechanism geometry**:

- **Central gear:** A large gear/axle at the junction point (center of the
  2x2 block). Rotates 1:1 with the platform (same angle, same direction)
  during animation, as if it's the drive shaft.
- **Satellite gears:** 2-4 smaller gears arranged around the center, meshed
  with the central gear. Rotate in the **opposite direction** at a higher
  rate (2x-3x the angle) to simulate mechanical linkage -- smaller gears
  spin faster when driven by a larger one.
- During rotation, the corners of the rotating square sweep past the platform
  edge, briefly exposing more of the mechanism underneath. This gives players
  a satisfying "peek behind the curtain" moment.
- When idle (no animation), the gears are stationary in their resting
  positions.

**Gear construction:** Each gear is a separate small `VoxelMesh` built at
level load time. A gear is approximated as a cluster of boxes: a central
cylinder (octagonal box stack) with rectangular cog teeth radiating outward
at regular intervals. Placed at Y = 0.02 to Y = ~0.08 (below the platform
surface, above the floor).

**Gear animation:** During the turnstile rotation tween, each gear gets its
own model matrix: `T(gear_center) * Ry(gear_angle) * T(-gear_center)`. The
`gear_angle` is derived from the same platform tween value:
- Central gear: `angle = platform_angle`
- Satellite gears: `angle = -platform_angle * (central_radius / satellite_radius)`

This reuses the exact same rendering pattern as the turnstile walls and
minotaur roll animation -- compute a per-frame model matrix, set `u_model`,
draw the mesh.

Color: dark metallic (0.30, 0.30, 0.33) for gear bodies, slightly lighter
(0.38, 0.38, 0.40) for cog teeth to catch the light.

## 4. Animation Design

### 4.1 Platform Rotation

The entire 2x2 platform surface rotates as a single rigid body around the
junction point. This is the key visual change -- the floor itself visibly
moves.

**Implementation:** The 4 floor tiles of the turnstile are generated into the
same **separate VoxelMesh** that already holds the turnstile walls (the
existing `turnstile_meshes[i]` system). This mesh already rotates around the
junction point via a model matrix during animation. Adding the floor tiles to
this mesh means they rotate for free.

- The floor tiles are **excluded** from the main diorama mesh (same filtering
  mechanism already used for turnstile walls via `DioramaExcludeSet`).
- The hazard stripes on the platform edges rotate with the platform.
- The gear mechanism underneath does **not** rotate (it's in the main diorama
  mesh), so during rotation the gears appear stationary while the platform
  spins above them.

### 4.2 Actor Arc Movement

Actors on turnstile tiles follow a **circular arc** around the junction point
instead of a linear interpolation between source and destination tiles.

**Current behavior** (linear):
```
position = lerp(from_tile, to_tile, t)
```

**New behavior** (arc):
```
// Actor's position relative to junction center
dx = from_col - junction_col
dz = from_row - junction_row

// Rotate by interpolated angle (0 to +/-90°)
angle = t * (pi/2) * direction  // +1 CW, -1 CCW
new_dx = dx * cos(angle) - dz * sin(angle)
new_dz = dx * sin(angle) + dz * cos(angle)

position = (junction_col + new_dx, junction_row + new_dz)
```

This means:
- An actor at NW tile (junction-relative position (-0.5, +0.5)) sweeps in an
  arc to NE (+0.5, +0.5) for CW rotation.
- The arc radius is `sqrt(0.5² + 0.5²) ≈ 0.707` tiles from the junction.
- The actor's center traces a quarter-circle, matching the platform rotation.

**Timing:** The actor arc and platform rotation use the **same tween**, so they
are perfectly synchronized. The actor appears to be "riding" the platform.

### 4.3 Easing and Polish

The rotation animation uses the existing turnstile easing curve:
- 0%--85% of duration: smooth rotation toward 90°
- 85%--100%: slight overshoot + snap-back oscillation for a mechanical
  "click into place" feel

Additional polish:
- **Landing wobble:** After rotation completes, actors on the platform get a
  brief squash-stretch wobble (reuse existing post-movement wobble system).
- **Gear animation:** Gears rotate around their own axes during the platform
  rotation (see §3.4). Central gear matches platform speed; satellite gears
  spin faster in the opposite direction.
- **Sound design hook:** The rotation should trigger a mechanical grinding/
  clicking sound effect (noted here for future audio implementation).

### 4.4 Shadow Behavior During Rotation

Actor shadows during turnstile rotation follow the same arc path as the actor.
Since the platform is raised (Y = 0.10), the shadow system already handles
multi-plane rendering (floor at Y = 0.01, elevated surface at
Y = CONVEYOR_BELT_TOP). The turnstile platform reuses this same elevation,
so shadows work correctly without additional changes.

## 5. Geometry Generation (`diorama_gen.c`)

### 5.1 New Generator: `gen_auto_turnstile_platform`

For each auto-turnstile feature, generate:

1. **Platform base** (4 boxes, one per tile):
   - Position: tile (col, row) at Y = 0.0
   - Size: 1.0 x CONVEYOR_HEIGHT x 1.0
   - Color: `platform_side` from biome palette
   - ao_mode: `AO_MODE_NONE` (sides are wall-heuristic darkened)

2. **Platform top surface** (4 boxes, one per tile):
   - Position: tile (col, row) at Y = CONVEYOR_HEIGHT
   - Size: 1.0 x 0.006 x 1.0 (thin cap, same as conveyor belt thickness)
   - Color: rigidized metal color (0.52, 0.52, 0.56)
   - ao_mode: `AO_MODE_TURNSTILE_PLATE` (new, for shader pattern) or
     `AO_MODE_LIGHTMAP` (if using standard floor lightmap)

3. **Hazard stripes** (outer perimeter edges only):
   - Same generation as conveyor hazard stripes
   - Only on edges facing away from the 2x2 interior
   - ao_mode: `AO_MODE_CONVEYOR_STRIPE`

4. **Gear meshes** (separate VoxelMeshes, animated independently):
   - **Central gear:** ~8-12 cog teeth boxes radiating from a central hub
     at the junction point. Hub approximated as an octagonal box stack.
     Y = 0.02 to Y = 0.08. Color: body (0.30, 0.30, 0.33), teeth
     (0.38, 0.38, 0.40).
   - **Satellite gears (2-4):** Smaller versions offset from center,
     positioned so their teeth visually mesh with the central gear.
     Each is its own VoxelMesh for independent rotation.

### 5.2 Mesh Assignment

| Component | Mesh | Rotates? |
|-----------|------|----------|
| Platform base (sides) | turnstile_meshes[i] | Yes (with platform) |
| Platform top surface | turnstile_meshes[i] | Yes (with platform) |
| Hazard stripes | turnstile_meshes[i] | Yes (with platform) |
| Walls | turnstile_meshes[i] | Yes (existing) |
| Central gear | turnstile_gear_meshes[i][0] | Yes (own axis, 1:1 with platform) |
| Satellite gears | turnstile_gear_meshes[i][1..N] | Yes (own axis, opposite direction, faster) |
| Normal floor under platform | excluded from both | N/A |

### 5.3 Floor Exclusion

The normal floor tiles for the 4 turnstile cells must be **excluded** from
`gen_floor`. Add the turnstile tile positions to the existing exclusion
system (same approach used for filtering turnstile walls).

## 6. Animation System Changes (`anim_queue.c`)

### 6.1 Arc Position Calculation

Replace the linear interpolation in the `AUTO_TURNSTILE_ROTATE` handler for
both Theseus and Minotaur position queries:

```c
/* In anim_queue_theseus_pos / anim_queue_minotaur_pos */
case ANIM_EVT_AUTO_TURNSTILE_ROTATE: {
    if (!cur->turnstile.actor_moved[actor_idx]) break;
    float t = tween_value(&aq->rotation);
    int jc = cur->turnstile.junction_col;
    int jr = cur->turnstile.junction_row;
    float fc = (float)cur->turnstile.actor_from_col[actor_idx] + 0.5f;
    float fr = (float)cur->turnstile.actor_from_row[actor_idx] + 0.5f;
    float dx = fc - (float)jc;
    float dz = fr - (float)jr;
    float sign = cur->turnstile.clockwise ? -1.0f : 1.0f;
    float angle = t * (M_PI * 0.5f) * sign;
    float cs = cosf(angle), sn = sinf(angle);
    *out_col = (float)jc + dx * cs - dz * sn - 0.5f;
    *out_row = (float)jr + dx * sn + dz * cs - 0.5f;
    *out_hop = 0.0f;
    break;
}
```

### 6.2 Timing

Keep the existing `ANIM_AUTO_TURNSTILE_DURATION = 0.15f`. The rotation is
fast and mechanical. If this feels too fast with the new visuals, increase
to 0.20--0.25s for better readability.

## 7. Renderer Changes

### 7.1 New ao_mode (Optional)

If using a procedural diamond-plate shader pattern:

```c
#define AO_MODE_TURNSTILE_PLATE 6
```

Fragment shader branch:
```glsl
if (v_ao_mode > 5.5) {
    /* Rigidized metal diamond plate pattern */
    vec2 dp = fract(v_world_pos.xz * 8.0);  /* 8 diamonds per tile */
    float diamond = abs(dp.x - 0.5) + abs(dp.y - 0.5);
    float bump = smoothstep(0.35, 0.5, diamond) * 0.06;
    base_color.rgb += bump;  /* subtle raised diamond pattern */
}
```

### 7.2 Conveyor Elevation Reuse

The existing `actor_conveyor_y()` function in `puzzle_scene.c` checks whether
an actor is on a conveyor tile and returns the elevation offset. Extend this
(or add a parallel function) to also return elevation for auto-turnstile tiles.
This ensures actors render at the correct height when standing on the platform.

## 8. Puzzle Scene Changes (`puzzle_scene.c`)

### 8.1 Turnstile Tile Map

Similar to the existing `conveyor_tile_map`, maintain a map of which tiles
are auto-turnstile tiles. This is used for:
- Actor elevation (standing on raised platform)
- Shadow multi-plane rendering
- Floor exclusion

### 8.2 Platform Mesh in Turnstile Meshes

The existing `turnstile_meshes[i]` array stores per-turnstile wall meshes
that rotate during animation. Extend the generation to include the platform
geometry (base, top surface, hazard stripes) in these same meshes.

After rotation animation completes, the mesh is already regenerated from
updated grid state (existing behavior at ~line 3000). The platform geometry
is static relative to the tiles, so regeneration produces correct results
regardless of accumulated rotation.

## 9. Implementation Order

1. **Geometry generation** -- `diorama_gen.c`:
   - Add `gen_auto_turnstile_platform()` function
   - Exclude turnstile floor tiles from `gen_floor()`
   - Generate platform base, top, and hazard stripes into turnstile mesh
   - Generate gear VoxelMeshes (central + satellites) as separate meshes

2. **Actor elevation** -- `puzzle_scene.c`:
   - Add turnstile tile map (or extend conveyor tile map)
   - Return `CONVEYOR_HEIGHT` for actors on turnstile tiles

3. **Arc animation** -- `anim_queue.c`:
   - Replace linear interpolation with arc calculation for actor positions
     during `AUTO_TURNSTILE_ROTATE` events

4. **Gear animation** -- `puzzle_scene.c`:
   - Store gear meshes and center positions per turnstile
   - During turnstile animation, compute per-gear model matrices from
     platform tween (central gear 1:1, satellites opposite & faster)
   - Draw each gear mesh with its own `u_model`

5. **Shader (optional)** -- `renderer.c`:
   - Add `AO_MODE_TURNSTILE_PLATE` branch for diamond-plate surface pattern

6. **Polish**:
   - Landing wobble after rotation
   - Direction indicator arrows
   - Tune animation duration

## 10. Verification

- Auto-turnstile tiles are visually distinct: raised metallic platform with
  hazard stripe border, clearly a 2x2 mechanical unit.
- During environment phase, the entire platform (floor + walls) rotates
  smoothly around the junction point.
- Actors on the platform follow a circular arc path, synchronized with the
  platform rotation. Movement looks like "riding a rotating platform."
- Actors not on the platform are unaffected.
- Gear mechanism is visible beneath the platform. Central gear rotates with
  the platform; satellite gears spin opposite and faster, simulating meshed
  mechanical linkage. Gears are stationary when no animation is playing.
- Shadows render correctly on both floor level and raised platform level.
- Actor elevation is correct when standing on turnstile tiles (same height
  as conveyor).
- After rotation completes, mesh regeneration produces correct geometry.
- Undo/redo correctly reverses turnstile state and actor positions.
- Multiple auto-turnstiles in the same level animate independently.
