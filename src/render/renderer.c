#include "render/renderer.h"
#include "render/shader.h"
#include "engine/engine.h"
#include "data/settings.h"

#include <string.h>
#include <math.h>

/* ---------- Shader sources ---------- */

static const char* s_ui_vert_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "uniform mat4 u_projection;\n"
    "uniform vec4 u_rect;\n"  /* x, y, w, h */
    "void main() {\n"
    "    vec2 pos = u_rect.xy + a_pos * u_rect.zw;\n"
    "    gl_Position = u_projection * vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char* s_ui_frag_src =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    FragColor = u_color;\n"
    "}\n";

static const char* s_ui_tex_vert_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "out vec2 v_uv;\n"
    "uniform mat4 u_projection;\n"
    "uniform vec4 u_rect;\n"
    "void main() {\n"
    "    vec2 pos = u_rect.xy + a_pos * u_rect.zw;\n"
    "    v_uv = a_pos;\n"
    "    gl_Position = u_projection * vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char* s_ui_tex_frag_src =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    float a = texture(u_texture, v_uv).r;\n"
    "    FragColor = vec4(u_color.rgb, u_color.a * a);\n"
    "}\n";

/* ---------- Voxel shader sources ---------- */

static const char* s_voxel_vert_src =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "layout(location = 3) in vec2 a_uv;\n"
    "layout(location = 4) in float a_ao_mode;\n"
    "\n"
    "uniform mat4 u_vp;\n"           /* view-projection */
    "uniform mat4 u_model;\n"        /* model transform */
    "\n"
    "/* Deformation uniforms (actor mesh jelly deformation).\n"
    " * When u_deform_height != 0, apply_deform() transforms the local\n"
    " * position before the model matrix.\n"
    " * Positive u_deform_height: also correct normals via finite-difference.\n"
    " * Negative u_deform_height: deform position only, keep original normals\n"
    " *   (avoids Mach banding at subdivision boundaries for subtle deforms). */\n"
    "uniform float u_deform_squash;\n"    /* Y-axis scale (1.0 = identity) */
    "uniform float u_deform_flare;\n"     /* bottom flare amount (0 = none) */
    "uniform vec2  u_deform_lean;\n"      /* XZ lean at top (0,0 = none) */
    "uniform vec2  u_deform_squish_dir;\n" /* normalized direction for directional squish */
    "uniform float u_deform_squish;\n"    /* squish amount along squish_dir (0 = none) */
    "uniform float u_deform_height;\n"    /* mesh height for t=y/h normalization (0 = disabled) */
    "\n"
    "vec3 apply_deform(vec3 p) {\n"
    "    float h = abs(u_deform_height);\n"
    "    float t = clamp(p.y / h, 0.0, 1.0);\n"
    "    /* Squash/stretch along Y with volume-preserving XZ compensation */\n"
    "    p.y *= u_deform_squash;\n"
    "    float inv_sq = 1.0 / sqrt(max(u_deform_squash, 0.01));\n"
    "    p.xz *= inv_sq;\n"
    "    /* Bottom flare: expand XZ at base, tapering to zero at top */\n"
    "    p.xz *= 1.0 + u_deform_flare * max(1.0 - t, 0.0);\n"
    "    /* Lean/shear: shift XZ proportional to height */\n"
    "    p.xz += u_deform_lean * t;\n"
    "    /* Directional squish: compress along an arbitrary horizontal axis */\n"
    "    float proj = dot(p.xz, u_deform_squish_dir);\n"
    "    p.xz -= u_deform_squish_dir * proj * u_deform_squish;\n"
    "    return p;\n"
    "}\n"
    "\n"
    "out vec3 v_world_pos;\n"
    "out vec3 v_normal;\n"
    "out vec4 v_color;\n"
    "out vec2 v_uv;\n"
    "out float v_ao_mode;\n"
    "noperspective out float v_height_frac;\n"  /* actor AO; noperspective avoids Mach bands at subdivision edges */
    "\n"
    "void main() {\n"
    "    vec3 local_pos = a_position;\n"
    "    vec3 local_nrm = a_normal;\n"
    "    v_height_frac = -1.0;\n"  /* -1 = not an actor */
    "\n"
    "    if (u_deform_height != 0.0) {\n"
    "        /* Compute height fraction from undeformed position for\n"
    "         * per-pixel ground-proximity AO in the fragment shader. */\n"
    "        v_height_frac = clamp(a_position.y / abs(u_deform_height), 0.0, 1.0);\n"
    "\n"
    "        /* Apply deformation to position */\n"
    "        local_pos = apply_deform(a_position);\n"
    "\n"
    "        /* Normal correction: only when u_deform_height > 0.\n"
    "         * Negative height = skip normals (avoids Mach banding\n"
    "         * at subdivision boundaries for subtle actor deforms). */\n"
    "        if (u_deform_height > 0.0) {\n"
    "            float eps = 0.001;\n"
    "            vec3 dx = apply_deform(a_position + vec3(eps,0,0))\n"
    "                    - apply_deform(a_position - vec3(eps,0,0));\n"
    "            vec3 dy = apply_deform(a_position + vec3(0,eps,0))\n"
    "                    - apply_deform(a_position - vec3(0,eps,0));\n"
    "            vec3 dz = apply_deform(a_position + vec3(0,0,eps))\n"
    "                    - apply_deform(a_position - vec3(0,0,eps));\n"
    "            vec3 j0 = dx; vec3 j1 = dy; vec3 j2 = dz;\n"
    "            vec3 cof0 = cross(j1, j2);\n"
    "            vec3 cof1 = cross(j2, j0);\n"
    "            vec3 cof2 = cross(j0, j1);\n"
    "            local_nrm = cof0 * a_normal.x + cof1 * a_normal.y + cof2 * a_normal.z;\n"
    "            local_nrm = normalize(local_nrm);\n"
    "        }\n"
    "    }\n"
    "\n"
    "    vec4 world = u_model * vec4(local_pos, 1.0);\n"
    "    v_world_pos = world.xyz;\n"
    "    v_normal = mat3(u_model) * local_nrm;\n"
    "    v_color = a_color;\n"
    "    v_uv = a_uv;\n"
    "    v_ao_mode = a_ao_mode;\n"
    "    gl_Position = u_vp * world;\n"
    "}\n";

static const char* s_voxel_frag_src =
    "#version 330 core\n"
    "\n"
    "in vec3 v_world_pos;\n"
    "in vec3 v_normal;\n"
    "in vec4 v_color;\n"
    "in vec2 v_uv;\n"
    "in float v_ao_mode;\n"
    "noperspective in float v_height_frac;\n"  /* actor per-pixel ground proximity (-1 = not actor) */
    "\n"
    "layout(location = 0) out vec4 FragColor;\n"
    "layout(location = 1) out vec4 NormalOut;\n"
    "\n"
    "/* Procedural noise utilities (used by floor grain + wall stone pattern).\n"
    " * Pure-arithmetic hash — avoids sin() which loses precision on some\n"
    " * GPUs when the input magnitude exceeds a few thousand. */\n"
    "float hash2d(vec2 p) {\n"
    "    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));\n"
    "    p3 += dot(p3, p3.yzx + 33.33);\n"
    "    return fract((p3.x + p3.y) * p3.z);\n"
    "}\n"
    "float value_noise(vec2 p) {\n"
    "    vec2 i = floor(p);\n"
    "    vec2 f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    float a = hash2d(i);\n"
    "    float b = hash2d(i + vec2(1.0, 0.0));\n"
    "    float c = hash2d(i + vec2(0.0, 1.0));\n"
    "    float d = hash2d(i + vec2(1.0, 1.0));\n"
    "    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
    "}\n"
    "float fbm(vec2 p) {\n"
    "    float v = 0.0;\n"
    "    float amp = 0.5;\n"
    "    for (int i = 0; i < 4; i++) {\n"
    "        v += amp * value_noise(p);\n"
    "        p *= 2.17;\n"
    "        amp *= 0.5;\n"
    "    }\n"
    "    return v;\n"
    "}\n"
    "\n"
    "/* Wall surface texture uniforms (per-biome, set from WallStyle)\n"
    " *   u_wall_stone_a: stone_height, stone_width_min, stone_width_max, mortar_width\n"
    " *   u_wall_stone_b: mortar_darkness, bevel_width, bevel_darkness, color_variation\n"
    " *   u_wall_stone_c: grain_intensity, grain_scale, wear, 0\n"
    " *   u_wall_stone_d: gap_color_r, gap_color_g, gap_color_b, 0 */\n"
    "uniform vec4 u_wall_stone_a;\n"
    "uniform vec4 u_wall_stone_b;\n"
    "uniform vec4 u_wall_stone_c;\n"
    "uniform vec4 u_wall_stone_d;\n"
    "\n"
    "/* Procedural wall stone pattern.\n"
    " * Projects a staggered brick layout onto a wall face, with:\n"
    " *   - Mortar lines (thin dark bands at stone boundaries)\n"
    " *   - Beveled/rounded edge shading (smoothstep gradient)\n"
    " *   - Per-stone color variation (hash-based tint shift)\n"
    " *   - Surface grain (FBM noise unique to each stone)\n"
    " * All parameters driven by u_wall_stone_* uniforms for per-biome control.\n"
    " *\n"
    " * Fitted slab layout: N slabs fill each wall segment exactly,\n"
    " * so no partial/narrow slabs appear at wall ends.\n"
    " * UV encoding: v_uv.x = local position along slab axis [0..seg_len],\n"
    " *              v_uv.y = orient * 100 + seg_len. */\n"
    "\n"
    "/* Fitted wall slab pattern. Takes base surface color and returns\n"
    " * the final color after applying mortar gap color, bevel darkening,\n"
    " * wear, per-slab variation, and grain. N = round(seg_len / avg_width) slabs\n"
    " * fill the segment exactly. No partial slabs at wall boundaries.\n"
    " * local_pos: interpolated position along slab axis [0..seg_len]\n"
    " * seg_len:   total wall segment length along slab axis\n"
    " * world_uv:  world-space coordinates for grain continuity */\n"
    "vec3 wall_stone(float local_pos, float seg_len, vec2 world_uv, vec3 base) {\n"
    "    float w_min   = u_wall_stone_a.y;\n"
    "    float w_max   = u_wall_stone_a.z;\n"
    "    float m_width = u_wall_stone_a.w;\n"
    "    float m_dark  = u_wall_stone_b.x;\n"
    "    float b_width = u_wall_stone_b.y;\n"
    "    float b_dark  = u_wall_stone_b.z;\n"
    "    float c_var   = u_wall_stone_b.w;\n"
    "    float g_int   = u_wall_stone_c.x;\n"
    "    float g_scale = u_wall_stone_c.y;\n"
    "    float wear    = u_wall_stone_c.z;\n"
    "    vec3 gap_col  = u_wall_stone_d.rgb;\n"
    "\n"
    "    /* Compute N slabs that fill the segment exactly */\n"
    "    float avg_w = (w_min + w_max) * 0.5;\n"
    "    float n = max(1.0, floor(seg_len / avg_w + 0.5));\n"
    "    float base_w = seg_len / n;\n"
    "\n"
    "    /* Seed for this segment's boundary jitter */\n"
    "    float seg_start = world_uv.x - local_pos;\n"
    "    float seed = seg_start * 17.3;\n"
    "\n"
    "    /* Jittered slab boundaries: each internal boundary is shifted\n"
    "     * randomly so slabs have varying widths. Endpoints stay fixed\n"
    "     * at 0 and seg_len so slabs always fill the segment exactly. */\n"
    "    float jitter_range = base_w * 0.35;\n"
    "    float col = clamp(floor(local_pos / base_w), 0.0, n - 1.0);\n"
    "\n"
    "    /* Compute jittered left and right boundaries for this slab */\n"
    "    float b_left  = (col > 0.0)\n"
    "        ? col * base_w + (hash2d(vec2(col, seed)) - 0.5) * 2.0 * jitter_range\n"
    "        : 0.0;\n"
    "    float b_right = (col < n - 1.0)\n"
    "        ? (col + 1.0) * base_w + (hash2d(vec2(col + 1.0, seed)) - 0.5) * 2.0 * jitter_range\n"
    "        : seg_len;\n"
    "\n"
    "    /* Re-check: jitter may push us into a neighbor slab */\n"
    "    if (local_pos < b_left && col > 0.0) {\n"
    "        col -= 1.0;\n"
    "        b_right = b_left;\n"
    "        b_left = (col > 0.0)\n"
    "            ? col * base_w + (hash2d(vec2(col, seed)) - 0.5) * 2.0 * jitter_range\n"
    "            : 0.0;\n"
    "    } else if (local_pos >= b_right && col < n - 1.0) {\n"
    "        col += 1.0;\n"
    "        b_left = b_right;\n"
    "        b_right = (col < n - 1.0)\n"
    "            ? (col + 1.0) * base_w + (hash2d(vec2(col + 1.0, seed)) - 0.5) * 2.0 * jitter_range\n"
    "            : seg_len;\n"
    "    }\n"
    "\n"
    "    float actual_w = b_right - b_left;\n"
    "    float local_x = clamp((local_pos - b_left) / actual_w, 0.0, 1.0);\n"
    "\n"
    "    /* Slab identity */\n"
    "    vec2 sid = vec2(col, seed);\n"
    "\n"
    "    /* Wear: perturb edge distance with noise so edges aren't perfectly straight.\n"
    "     * Higher wear = more irregular, chipped-looking boundaries. */\n"
    "    float edge_dist = min(local_x, 1.0 - local_x) * 2.0;\n"
    "    if (wear > 0.0) {\n"
    "        /* High-frequency noise along the edge for irregularity */\n"
    "        float edge_noise = fbm(world_uv * 30.0) * 2.0 - 1.0;\n"
    "        edge_dist += edge_noise * wear * 0.15;\n"
    "        edge_dist = clamp(edge_dist, 0.0, 1.0);\n"
    "    }\n"
    "\n"
    "    /* Mortar: at gap boundaries, blend base color toward gap_color.\n"
    "     * mortar = 0 at slab edge, 1 at slab center.\n"
    "     * gap_blend = strength of gap color (0 at center, m_dark at edge). */\n"
    "    float mortar = smoothstep(0.0, m_width, edge_dist);\n"
    "    float gap_blend = (1.0 - mortar) * m_dark;\n"
    "\n"
    "    /* Bevel: wider gradient simulating rounded slab edges.\n"
    "     * Wear widens the bevel to simulate erosion.\n"
    "     * Bevel also blends toward gap_color (softer than mortar). */\n"
    "    float worn_bevel = b_width + wear * 0.15;\n"
    "    float bevel = smoothstep(0.0, worn_bevel, edge_dist);\n"
    "    float bevel_blend = (1.0 - bevel) * b_dark;\n"
    "\n"
    "    /* Wear highlight: brighten the slab near edges on the upper\n"
    "     * part (simulates light catching rounded/convex worn surfaces).\n"
    "     * Uses world Y (or secondary UV axis) for vertical position. */\n"
    "    float highlight = 1.0;\n"
    "    if (wear > 0.0) {\n"
    "        float near_edge = 1.0 - smoothstep(0.0, worn_bevel * 1.5, edge_dist);\n"
    "        float vert_frac = clamp(world_uv.y / 0.30, 0.0, 1.0);\n"
    "        /* Upper portion: subtle highlight; lower: subtle shadow */\n"
    "        float edge_light = mix(-0.06, 0.08, vert_frac);\n"
    "        highlight = 1.0 + near_edge * edge_light * wear;\n"
    "    }\n"
    "\n"
    "    /* Per-slab color variation (multiplicative, centered at 1.0) */\n"
    "    float color_var = 1.0 + (hash2d(sid + vec2(42.0, 17.0)) - 0.5) * c_var;\n"
    "\n"
    "    /* Surface grain (FBM noise unique to each slab).\n"
    "     * Offset keeps grain different per slab; kept small to avoid\n"
    "     * high-frequency aliasing artifacts (Moire stripes). */\n"
    "    vec2 grain_off = vec2(hash2d(sid), hash2d(sid + vec2(73.0, 157.0))) * 7.0;\n"
    "    float grain = fbm((world_uv + grain_off) * g_scale);\n"
    "    float grain_mod = mix(1.0 - g_int, 1.0 + g_int, grain);\n"
    "\n"
    "    /* Compose: blend toward gap_color at mortar+bevel, then apply\n"
    "     * highlight, per-slab variation, and grain multiplicatively. */\n"
    "    float total_gap = clamp(gap_blend + bevel_blend, 0.0, 1.0);\n"
    "    vec3 color = mix(base, gap_col, total_gap);\n"
    "    color *= highlight * color_var * grain_mod;\n"
    "    return color;\n"
    "}\n"
    "\n"
    "/* Directional light */\n"
    "uniform vec4 u_light_dir;\n"      /* xyz = direction TO light */
    "uniform vec4 u_light_color;\n"    /* rgb = color */
    "uniform vec4 u_ambient_color;\n"  /* rgb = ambient */
    "\n"
    "/* Point lights */\n"
    "uniform int  u_point_count;\n"
    "uniform vec4 u_point_pos[8];\n"
    "uniform vec4 u_point_color[8];\n"
    "uniform float u_point_radius[8];\n"
    "\n"
    "/* AO texture atlas (unit 0) */\n"
    "uniform sampler2D u_ao_texture;\n"
    "uniform int u_has_ao;\n"          /* 1 if AO texture is bound */
    "uniform float u_ao_intensity;\n"  /* 0..1: lerp between no-AO and full-AO */
    "\n"
    "/* Floor lightmap (unit 1) */\n"
    "uniform sampler2D u_floor_lightmap;\n"
    "uniform vec4 u_lightmap_bounds;\n"  /* origin_x, origin_z, extent_x, extent_z */
    "\n"
    "/* Cel-shading toggle */\n"
    "uniform int u_cel_shading;\n"  /* 0 = standard, 1 = cel-shaded */
    "\n"
    "/* Actor ground-proximity AO (world-space approach).\n"
    " * u_actor_ground_y: world Y of the floor under the actor.\n"
    " * u_actor_height:   actor mesh height (for normalizing to 0..1).\n"
    " * When u_actor_height <= 0, these are unused (not an actor draw). */\n"
    "uniform float u_actor_ground_y;\n"
    "uniform float u_actor_height;\n"
    "\n"
    "/* Conveyor belt animation */\n"
    "uniform float u_conveyor_scroll;\n"  /* 0..1 scroll progress during push animation */
    "uniform vec2  u_conveyor_dir;\n"     /* (dx, dz) belt direction in world space */
    "\n"
    "void main() {\n"
    "    vec3 N = normalize(v_normal);\n"
    "    vec3 base_color = v_color.rgb;\n"
    "\n"
    "    /* Multi-tier AO / material system:\n"
    "     *   ao_mode 7:   turnstile diamond-plate surface\n"
    "     *   ao_mode 4-6: conveyor belt / hazard stripes / rail\n"
    "     *   ao_mode 3:   actor shadow\n"
    "     *   ao_mode 2:   floor lightmap\n"
    "     *   ao_mode 1:   raytraced AO atlas\n"
    "     *   ao_mode 0:   wall heuristic (vertex color pre-darkened) */\n"
    "    float ao = 1.0;\n"
    "    if (v_ao_mode > 3.5) {\n"
    "        if (v_ao_mode > 6.5) {\n"
    "            /* AO_MODE_TURNSTILE_PLATE (7): raised metal platform surface.\n"
    "             * Procedural diamond-plate bump pattern for industrial look.\n"
    "             * No lightmap AO — platform is elevated, rendered in a separate\n"
    "             * mesh that doesn't carry a floor lightmap texture. */\n"
    "            vec3 abs_n = abs(N);\n"
    "            if (abs_n.y > 0.5) {\n"
    "                /* Top face: diamond plate pattern */\n"
    "                vec2 dp = fract(v_world_pos.xz * 10.0);\n"
    "                float diamond = abs(dp.x - 0.5) + abs(dp.y - 0.5);\n"
    "                float bump = smoothstep(0.30, 0.50, diamond) * 0.08;\n"
    "                base_color.rgb += bump;\n"
    "                /* Subtle cross-hatch grain for metal texture */\n"
    "                float grain = sin(v_world_pos.x * 80.0) * sin(v_world_pos.z * 80.0) * 0.015;\n"
    "                base_color.rgb += grain;\n"
    "            }\n"
    "            /* ao stays 1.0 — no floor shadow on elevated platform */\n"
    "        } else if (v_ao_mode > 5.5) {\n"
    "            /* AO_MODE_CONVEYOR_RAIL (6): metallic rail / roller drum.\n"
    "             * Uses vertex color for appearance, floor lightmap for AO.\n"
    "             * Same as AO_MODE_LIGHTMAP but tagged as conveyor for outline. */\n"
    "            vec2 lm_uv = (v_world_pos.xz - u_lightmap_bounds.xy) / u_lightmap_bounds.zw;\n"
    "            float lm_sample = texture(u_floor_lightmap, lm_uv).r;\n"
    "            ao = mix(1.0, lm_sample, u_ao_intensity);\n"
    "        } else if (v_ao_mode > 4.5) {\n"
    "            /* AO_MODE_CONVEYOR_STRIPE (5): hazard stripes on side walls.\n"
    "             * Diagonal yellow/dark-gray bands on non-top faces.\n"
    "             * Top face gets flat dark color. */\n"
    "            vec3 abs_n = abs(N);\n"
    "            if (abs_n.y > 0.5) {\n"
    "                /* Top face: dark metal platform.\n"
    "                 * Perturb the normal slightly so the outline pass\n"
    "                 * detects an edge against the adjacent floor tile\n"
    "                 * (both are upward-facing, but the depth difference\n"
    "                 * is too small for the depth threshold). */\n"
    "                base_color = vec3(0.30, 0.30, 0.32);\n"
    "            } else {\n"
    "                /* Side face: diagonal hazard stripes */\n"
    "                float along;\n"
    "                if (abs_n.x > 0.5) {\n"
    "                    along = v_world_pos.z + v_world_pos.y;\n"
    "                } else {\n"
    "                    along = v_world_pos.x + v_world_pos.y;\n"
    "                }\n"
    "                float stripe = step(0.5, fract(along * 8.0));\n"
    "                vec3 col_a = vec3(0.85, 0.75, 0.15);\n"  /* yellow */
    "                vec3 col_b = vec3(0.20, 0.20, 0.22);\n"  /* dark gray */
    "                base_color = mix(col_a, col_b, stripe);\n"
    "            }\n"
    "            /* Sample floor lightmap for AO on conveyor surface */\n"
    "            vec2 lm_uv = (v_world_pos.xz - u_lightmap_bounds.xy) / u_lightmap_bounds.zw;\n"
    "            float lm_sample = texture(u_floor_lightmap, lm_uv).r;\n"
    "            ao = mix(1.0, lm_sample, u_ao_intensity);\n"
    "        } else {\n"
    "            /* AO_MODE_CONVEYOR_BELT (4): scrolling belt surface.\n"
    "             * Dark rubber material with cross-ridges that scroll\n"
    "             * during conveyor push animation via u_conveyor_scroll.\n"
    "             * v_uv.x encodes belt axis: 0=horiz (E/W), 1=vert (N/S).\n"
    "             * v_uv.y encodes signed direction: +1 or -1 for scroll sign. */\n"
    "            vec3 abs_n = abs(N);\n"
    "            if (abs_n.y > 0.5) {\n"
    "                /* Top face: belt with ridges */\n"
    "                float belt_axis = v_uv.x;\n"
    "                float is_vert = step(0.5, belt_axis);\n"
    "                vec2 belt_dir = mix(vec2(1.0, 0.0), vec2(0.0, 1.0), is_vert);\n"
    "                float dir_sign = v_uv.y;\n"
    "                float along = dot(v_world_pos.xz, belt_dir);\n"
    "                /* Flip phase for vertical belts so specular highlight\n"
    "                 * faces north (camera-facing side in isometric view). */\n"
    "                along = mix(along, -along, is_vert);\n"
    "                float scrolled = along - u_conveyor_scroll * dir_sign;\n"
    "\n"
    "                /* Cross-ridges: repeating slats with gaps.\n"
    "                 * Each slat gets a light leading edge (specular highlight)\n"
    "                 * and a dark trailing edge to simulate rounded cross-section. */\n"
    "                float slat_phase = fract(scrolled * 6.0);\n"
    "\n"
    "                /* Gap between slats (dark groove) */\n"
    "                float gap = smoothstep(0.0, 0.04, slat_phase)\n"
    "                          - smoothstep(0.10, 0.14, slat_phase);\n"
    "\n"
    "                /* Specular highlight on leading edge of slat */\n"
    "                float highlight = smoothstep(0.14, 0.18, slat_phase)\n"
    "                                - smoothstep(0.18, 0.26, slat_phase);\n"
    "\n"
    "                /* Shadow on trailing edge of slat */\n"
    "                float shadow_edge = smoothstep(0.85, 0.92, slat_phase)\n"
    "                             - smoothstep(0.92, 1.00, slat_phase);\n"
    "\n"
    "                vec3 belt_slat = vec3(0.25, 0.25, 0.27);\n"
    "                vec3 belt_gap  = vec3(0.04, 0.04, 0.05);\n"
    "                base_color = mix(belt_slat, belt_gap, gap);\n"
    "                base_color += vec3(0.12, 0.11, 0.10) * highlight;\n"  /* warm highlight */
    "                base_color -= vec3(0.08) * shadow_edge;\n"  /* subtle darkening */
    "\n"
    "                /* Subtle side darkening near rail edges */\n"
    "                vec2 perp_dir = vec2(-belt_dir.y, belt_dir.x);\n"
    "                float perp = dot(v_world_pos.xz, perp_dir);\n"
    "                float tile_frac = fract(perp);\n"
    "                float edge_dark = smoothstep(0.0, 0.12, min(tile_frac, 1.0 - tile_frac));\n"
    "                base_color *= mix(0.6, 1.0, edge_dark);\n"
    "            } else {\n"
    "                /* Side face: shouldn't be visible but dark rubber */\n"
    "                base_color = vec3(0.15, 0.15, 0.17);\n"
    "            }\n"
    "            /* Floor lightmap AO */\n"
    "            vec2 lm_uv = (v_world_pos.xz - u_lightmap_bounds.xy) / u_lightmap_bounds.zw;\n"
    "            float lm_sample = texture(u_floor_lightmap, lm_uv).r;\n"
    "            ao = mix(1.0, lm_sample, u_ao_intensity);\n"
    "        }\n"
    "    } else if (v_ao_mode > 2.5) {\n"
    "        /* Actor shadow: sample shadow texture (on unit 1) as alpha.\n"
    "         * UVs map [0,1] across the shadow texture. Output black\n"
    "         * with alpha from texture; blending darkens the floor. */\n"
    "        float shadow_val = texture(u_floor_lightmap, v_uv).r;\n"
    "        FragColor = vec4(0.0, 0.0, 0.0, 1.0 - shadow_val);\n"
    "        NormalOut = vec4(0.5, 1.0, 0.5, 0.0);\n"  /* up-facing normal, alpha=0 marks non-geometry */
    "        return;\n"
    "    } else if (v_ao_mode > 1.5) {\n"
    "        /* Floor lightmap */\n"
    "        vec2 lm_uv = (v_world_pos.xz - u_lightmap_bounds.xy) / u_lightmap_bounds.zw;\n"
    "        float lm_sample = texture(u_floor_lightmap, lm_uv).r;\n"
    "        ao = mix(1.0, lm_sample, u_ao_intensity);\n"
    "\n"
    "        /* Procedural stone grain: per-tile random offset into shared FBM field */\n"
    "        vec2 tile_id = floor(v_world_pos.xz);\n"
    "        vec2 tile_offset = vec2(hash2d(tile_id), hash2d(tile_id + vec2(73.0, 157.0))) * 100.0;\n"
    "        vec2 local_uv = fract(v_world_pos.xz);\n"
    "        float stone = fbm((local_uv + tile_offset) * 6.0);\n"
    "        base_color *= mix(0.93, 1.03, stone);\n"
    "    } else if (v_ao_mode > 0.5) {\n"
    "        if (v_height_frac >= 0.0) {\n"
    "            /* Actor per-pixel ground-proximity AO.\n"
    "             * Use world-space Y relative to the actor's ground plane\n"
    "             * so the shadow band tracks the actual floor contact,\n"
    "             * not the mesh's local orientation (avoids band rotating\n"
    "             * with the actor during roll animations). */\n"
    "            float hf = (u_actor_height > 0.0)\n"
    "                ? clamp((v_world_pos.y - u_actor_ground_y) / u_actor_height, 0.0, 1.0)\n"
    "                : clamp(v_height_frac, 0.0, 1.0);\n"
    "            float abs_ny = abs(N.y);\n"
    "            float face_ao;\n"
    "            if (u_cel_shading != 0) {\n"
    "                /* Cel-shading: hard shadow band at bottom of side faces */\n"
    "                if (abs_ny > 0.5) {\n"
    "                    face_ao = (N.y > 0.0) ? 1.0 : 0.70;\n"
    "                } else {\n"
    "                    face_ao = (hf < 0.18) ? 0.65 : 1.0;\n"
    "                }\n"
    "            } else {\n"
    "                /* Standard: smooth gradient */\n"
    "                float ground_dark = 0.30;\n"
    "                float ground_range = 0.50;\n"
    "                if (abs_ny > 0.5) {\n"
    "                    face_ao = (N.y > 0.0) ? 1.0 : (1.0 - ground_dark * 0.6);\n"
    "                } else {\n"
    "                    float t = smoothstep(0.0, ground_range, hf);\n"
    "                    face_ao = 1.0 - ground_dark * (1.0 - t);\n"
    "                }\n"
    "            }\n"
    "            ao = mix(1.0, face_ao, u_ao_intensity);\n"
    "        } else if (u_has_ao != 0) {\n"
    "            /* Raytraced AO atlas (complex geometry) */\n"
    "            float ao_sample = texture(u_ao_texture, v_uv).r;\n"
    "            if (u_cel_shading != 0) {\n"
    "                /* Quantize AO to binary for cel-shaded look */\n"
    "                ao_sample = (ao_sample < 0.7) ? 0.55 : 1.0;\n"
    "            }\n"
    "            ao = mix(1.0, ao_sample, u_ao_intensity);\n"
    "        }\n"
    "    } else {\n"
    "        /* ao_mode=0: wall heuristic (vertex already darkened) +\n"
    "         * procedural stone pattern for surface detail.\n"
    "         * UV encoding: v_uv.x = local slab-axis position,\n"
    "         *              v_uv.y = orient * 100 + seg_len */\n"
    "        float w_orient = floor(v_uv.y * 0.01);\n"
    "        float seg_len  = v_uv.y - w_orient * 100.0;\n"
    "        float local_pos = v_uv.x;\n"
    "        vec3 abs_n = abs(N);\n"
    "\n"
    "        if (w_orient > 1.5) {\n"
    "            /* Corner block: too small for slab boundaries.\n"
    "             * Apply only grain (no mortar/bevel edges). */\n"
    "            float g_int   = u_wall_stone_c.x;\n"
    "            float g_scale = u_wall_stone_c.y;\n"
    "            float grain = fbm(v_world_pos.xz * g_scale);\n"
    "            base_color *= mix(1.0 - g_int, 1.0 + g_int, grain);\n"
    "        } else {\n"
    "            /* Wall segment: fitted slabs */\n"
    "            vec2 world_uv;\n"
    "            if (abs_n.y > 0.5) {\n"
    "                /* Top face */\n"
    "                world_uv = (w_orient > 0.5)\n"
    "                    ? vec2(v_world_pos.z, v_world_pos.x)\n"
    "                    : vec2(v_world_pos.x, v_world_pos.z);\n"
    "            } else {\n"
    "                /* Side face */\n"
    "                world_uv = (w_orient > 0.5)\n"
    "                    ? vec2(v_world_pos.z, v_world_pos.y)\n"
    "                    : vec2(v_world_pos.x, v_world_pos.y);\n"
    "            }\n"
    "            base_color = wall_stone(local_pos, seg_len, world_uv, base_color);\n"
    "        }\n"
    "        /* Cel-shading: hard shadow band at bottom of wall side faces.\n"
    "         * Applied after stone pattern so texture is preserved. */\n"
    "        if (u_cel_shading != 0 && abs_n.y < 0.5) {\n"
    "            float wall_frac = v_world_pos.y / 0.30;\n"
    "            base_color *= (wall_frac < 0.18) ? 0.65 : 1.0;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    /* Apply AO to base color */\n"
    "    base_color *= ao;\n"
    "    float final_alpha = v_color.a;\n"
    "\n"
    "    /* Ambient */\n"
    "    vec3 lit = u_ambient_color.rgb * base_color;\n"
    "\n"
    "    /* Directional diffuse */\n"
    "    float ndl = dot(N, u_light_dir.xyz);\n"
    "    float diffuse;\n"
    "    if (u_cel_shading != 0) {\n"
    "        /* Cel-shaded: 3-band quantization with hard edges */\n"
    "        float raw = ndl * 0.5 + 0.5;\n"  /* half-Lambert base */
    "        diffuse = floor(raw * 3.0 + 0.5) / 3.0;\n"
    "    } else {\n"
    "        diffuse = ndl * 0.5 + 0.5;\n"  /* smooth half-Lambert */
    "    }\n"
    "    lit += u_light_color.rgb * base_color * diffuse;\n"
    "\n"
    "    /* Point lights */\n"
    "    for (int i = 0; i < u_point_count; i++) {\n"
    "        vec3 to_light = u_point_pos[i].xyz - v_world_pos;\n"
    "        float dist = length(to_light);\n"
    "        float atten = max(0.0, 1.0 - dist / u_point_radius[i]);\n"
    "        atten *= atten;\n"  /* quadratic falloff */
    "        float pndl = max(0.0, dot(N, normalize(to_light)));\n"
    "        if (u_cel_shading != 0) {\n"
    "            /* Quantize point light contribution */\n"
    "            pndl = floor(pndl * 2.0 + 0.5) / 2.0;\n"
    "        }\n"
    "        lit += u_point_color[i].rgb * base_color * pndl * atten;\n"
    "    }\n"
    "\n"
    "    /* Cel-shading: optional rim highlight */\n"
    "    if (u_cel_shading != 0) {\n"
    "        /* Approximate view direction for isometric camera.\n"
    "         * Uses the light direction reflected — gives a subtle\n"
    "         * bright edge on faces angled away from the light. */\n"
    "        float rim = 1.0 - max(0.0, dot(N, vec3(0.0, 1.0, 0.0)));\n"
    "        rim = smoothstep(0.6, 0.9, rim);\n"
    "        lit += rim * 0.08 * base_color;\n"
    "    }\n"
    "\n"
    "    FragColor = vec4(lit, final_alpha);\n"
    "    /* Alpha = 0.5 for conveyor surfaces, 1.0 for regular geometry.\n"
    "     * The outline shader detects alpha discontinuities to draw\n"
    "     * cel-shading edges around conveyor platforms. */\n"
    "    float norm_alpha = (v_ao_mode > 3.5) ? 0.5 : 1.0;\n"
    "    NormalOut = vec4(N * 0.5 + 0.5, norm_alpha);\n"
    "}\n";

/* ---------- Outline post-process shader ---------- */

/* Fullscreen vertex shader — renders a triangle that covers the screen.
 * Uses vertex ID trick: no VBO needed. */
static const char* s_outline_vert_src =
    "#version 330 core\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    /* Full-screen triangle from vertex ID (0,1,2) */\n"
    "    v_uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "    gl_Position = vec4(v_uv * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

/* Outline fragment shader — reads color, depth, and normal textures,
 * detects edges via Sobel depth gradient + normal discontinuity,
 * and composites black outlines over the scene. */
static const char* s_outline_frag_src =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "out vec4 FragColor;\n"
    "\n"
    "uniform sampler2D u_color_tex;\n"   /* scene color */
    "uniform sampler2D u_normal_tex;\n"  /* encoded normals */
    "uniform sampler2D u_depth_tex;\n"   /* depth buffer */
    "uniform vec2 u_texel_size;\n"       /* 1.0 / resolution */
    "uniform float u_depth_threshold;\n" /* edge sensitivity for depth */
    "uniform float u_normal_threshold;\n" /* edge sensitivity for normals */
    "uniform float u_near;\n"            /* camera near plane */
    "uniform float u_far;\n"             /* camera far plane */
    "uniform int u_ortho;\n"             /* 1 = orthographic, 0 = perspective */
    "\n"
    "/* Linearize depth from NDC depth buffer value.\n"
    " * In perspective: non-linear mapping requires the standard formula.\n"
    " * In ortho: depth buffer is already linear in [0,1] — use as-is. */\n"
    "float linearize_depth(float d) {\n"
    "    if (u_ortho != 0) {\n"
    "        return d;\n"  /* already linear, [0,1] range matches threshold scale */
    "    }\n"
    "    return (2.0 * u_near) / (u_far + u_near - d * (u_far - u_near));\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec4 scene = texture(u_color_tex, v_uv);\n"
    "\n"
    "    /* Sample depth at cardinal neighbors */\n"
    "    float d_c = linearize_depth(texture(u_depth_tex, v_uv).r);\n"
    "    float d_l = linearize_depth(texture(u_depth_tex, v_uv + vec2(-u_texel_size.x, 0.0)).r);\n"
    "    float d_r = linearize_depth(texture(u_depth_tex, v_uv + vec2( u_texel_size.x, 0.0)).r);\n"
    "    float d_u = linearize_depth(texture(u_depth_tex, v_uv + vec2(0.0,  u_texel_size.y)).r);\n"
    "    float d_d = linearize_depth(texture(u_depth_tex, v_uv + vec2(0.0, -u_texel_size.y)).r);\n"
    "\n"
    "    /* Sobel-like depth gradient */\n"
    "    float depth_edge = abs(d_l - d_r) + abs(d_u - d_d);\n"
    "    depth_edge = smoothstep(u_depth_threshold * 0.5, u_depth_threshold, depth_edge);\n"
    "\n"
    "    /* Sample normals at cardinal neighbors.\n"
    "     * Normalize after decoding — MRT alpha blending can scale the stored\n"
    "     * RGB values (when NormalOut.a < 1), making them non-unit length.\n"
    "     * Without normalization, dot(n,n) < 1 for blended normals, which\n"
    "     * falsely triggers normal-edge detection on every such pixel. */\n"
    "    vec3 n_c = normalize(texture(u_normal_tex, v_uv).rgb * 2.0 - 1.0);\n"
    "    vec3 n_l = normalize(texture(u_normal_tex, v_uv + vec2(-u_texel_size.x, 0.0)).rgb * 2.0 - 1.0);\n"
    "    vec3 n_r = normalize(texture(u_normal_tex, v_uv + vec2( u_texel_size.x, 0.0)).rgb * 2.0 - 1.0);\n"
    "    vec3 n_u = normalize(texture(u_normal_tex, v_uv + vec2(0.0,  u_texel_size.y)).rgb * 2.0 - 1.0);\n"
    "    vec3 n_d = normalize(texture(u_normal_tex, v_uv + vec2(0.0, -u_texel_size.y)).rgb * 2.0 - 1.0);\n"
    "\n"
    "    /* Normal discontinuity: how different are neighboring normals? */\n"
    "    float normal_edge = 0.0;\n"
    "    normal_edge += 1.0 - dot(n_c, n_l);\n"
    "    normal_edge += 1.0 - dot(n_c, n_r);\n"
    "    normal_edge += 1.0 - dot(n_c, n_u);\n"
    "    normal_edge += 1.0 - dot(n_c, n_d);\n"
    "    normal_edge *= 0.25;\n"  /* average */
    "    normal_edge = smoothstep(u_normal_threshold * 0.5, u_normal_threshold, normal_edge);\n"
    "\n"
    "    /* Material-boundary edge: detect conveyor platforms via NormalOut alpha.\n"
    "     * Conveyor surfaces write alpha=0.5, regular geometry writes 1.0,\n"
    "     * non-geometry (shadows, sky) writes 0.0.  A large alpha difference\n"
    "     * between neighbors indicates a conveyor-to-floor boundary. */\n"
    "    float a_c = texture(u_normal_tex, v_uv).a;\n"
    "    float a_l = texture(u_normal_tex, v_uv + vec2(-u_texel_size.x, 0.0)).a;\n"
    "    float a_r = texture(u_normal_tex, v_uv + vec2( u_texel_size.x, 0.0)).a;\n"
    "    float a_u = texture(u_normal_tex, v_uv + vec2(0.0,  u_texel_size.y)).a;\n"
    "    float a_d = texture(u_normal_tex, v_uv + vec2(0.0, -u_texel_size.y)).a;\n"
    "    float mat_edge = max(max(abs(a_c - a_l), abs(a_c - a_r)),\n"
    "                         max(abs(a_c - a_u), abs(a_c - a_d)));\n"
    "    /* Only trigger at real geometry boundaries (skip non-geometry alpha=0) */\n"
    "    mat_edge *= step(0.1, a_c);\n"
    "    mat_edge = smoothstep(0.15, 0.35, mat_edge);\n"
    "\n"
    "    /* Combine depth, normal, and material edges */\n"
    "    float edge = max(max(depth_edge, normal_edge), mat_edge);\n"
    "    edge = clamp(edge, 0.0, 1.0);\n"
    "\n"
    "    /* Darken toward black at edges */\n"
    "    vec3 outline_color = vec3(0.02, 0.02, 0.04);\n"  /* near-black with slight blue tint */
    "    vec3 result = mix(scene.rgb, outline_color, edge);\n"
    "    FragColor = vec4(result, 1.0);\n"  /* force opaque — shadow blending contaminates FBO alpha */
    "}\n";

/* ---------- State ---------- */

static struct {
    GLuint ui_shader;
    GLuint ui_tex_shader;
    GLuint voxel_shader;
    GLuint outline_shader;
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint fallback_tex;    /* 1×1 white texture — keeps samplers valid */
    float  projection[16];

    /* Outline post-process FBO */
    GLuint outline_fbo;
    GLuint outline_color_tex;
    GLuint outline_normal_tex;
    GLuint outline_depth_tex;
    GLuint outline_dummy_vao;   /* empty VAO for fullscreen triangle */
    int    outline_width;
    int    outline_height;
} s_renderer;

/* ---------- Outline FBO management ---------- */

static void outline_fbo_ensure(int w, int h) {
    if (s_renderer.outline_fbo && s_renderer.outline_width == w &&
        s_renderer.outline_height == h) {
        return;  /* already correct size */
    }

    /* Delete old textures if resizing */
    if (s_renderer.outline_color_tex)  glDeleteTextures(1, &s_renderer.outline_color_tex);
    if (s_renderer.outline_normal_tex) glDeleteTextures(1, &s_renderer.outline_normal_tex);
    if (s_renderer.outline_depth_tex)  glDeleteTextures(1, &s_renderer.outline_depth_tex);

    /* Create FBO on first call */
    if (!s_renderer.outline_fbo) {
        glGenFramebuffers(1, &s_renderer.outline_fbo);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, s_renderer.outline_fbo);

    /* Color attachment 0: scene color (RGBA8) */
    glGenTextures(1, &s_renderer.outline_color_tex);
    glBindTexture(GL_TEXTURE_2D, s_renderer.outline_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s_renderer.outline_color_tex, 0);

    /* Color attachment 1: normals (RGB8 — sufficient precision for edge detection) */
    glGenTextures(1, &s_renderer.outline_normal_tex);
    glBindTexture(GL_TEXTURE_2D, s_renderer.outline_normal_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           s_renderer.outline_normal_tex, 0);

    /* Depth+stencil attachment (24-bit depth + 8-bit stencil).
     * Stencil is used by multi-plane shadow rendering to prevent
     * double-darkening when floor and conveyor shadows overlap. */
    glGenTextures(1, &s_renderer.outline_depth_tex);
    glBindTexture(GL_TEXTURE_2D, s_renderer.outline_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0,
                 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, s_renderer.outline_depth_tex, 0);

    /* Enable both color attachments for MRT */
    GLenum draw_bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, draw_bufs);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("Outline FBO incomplete: 0x%x", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    s_renderer.outline_width = w;
    s_renderer.outline_height = h;
}

/* Build an orthographic projection matrix (column-major). */
static void build_ortho(float* out, float left, float right,
                        float bottom, float top,
                        float near, float far) {
    memset(out, 0, 16 * sizeof(float));
    out[0]  = 2.0f / (right - left);
    out[5]  = 2.0f / (top - bottom);
    out[10] = -2.0f / (far - near);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(far + near) / (far - near);
    out[15] = 1.0f;
}

/*
 * Build a combined perspective × view matrix that matches the ortho view at z=0.
 *
 * Camera sits at (w/2, h/2, D) looking at (w/2, h/2, 0), where
 * D = (h/2) / tan(fov/2). At z=0, this covers exactly the same region
 * as the orthographic projection, so all existing 2D content renders
 * pixel-identically. The perspective effect only becomes visible when
 * geometry has varying Z depth (e.g. zoom transitions).
 *
 * The view matrix flips Y to match screen-space convention (Y-down,
 * origin at top-left), matching the ortho matrix behavior.
 */
static void build_perspective_view(float* out, float w, float h, float fov_deg) {
    float fov_rad = fov_deg * ((float)M_PI / 180.0f);
    float half_fov = fov_rad * 0.5f;
    float cam_dist = (h * 0.5f) / tanf(half_fov);

    float aspect = w / h;
    float near_z = 1.0f;
    float far_z  = cam_dist * 2.0f;

    /* ── Perspective matrix P (column-major) ─────────────── */
    float f = 1.0f / tanf(half_fov);
    float P[16];
    memset(P, 0, sizeof(P));
    P[0]  = f / aspect;
    P[5]  = f;
    P[10] = -(far_z + near_z) / (far_z - near_z);
    P[11] = -1.0f;
    P[14] = -(2.0f * far_z * near_z) / (far_z - near_z);

    /* ── View matrix V (column-major) ────────────────────── */
    /*
     * Camera at eye = (w/2, h/2, cam_dist), looking at center = (w/2, h/2, 0).
     * Forward = (0, 0, -1), Right = (1, 0, 0), Up = (0, 1, 0).
     *
     * But our screen convention is Y-down (origin top-left), so we flip
     * the Y axis: Up = (0, -1, 0). This makes the view matrix:
     *
     *   R:  (1,  0, 0)    T: (-w/2)
     *   U:  (0, -1, 0)    T: ( h/2)
     *   F:  (0,  0, 1)    T: (-cam_dist)
     *
     * Column-major layout:
     *   col0 = (Rx, Ux, -Fx, 0) = (1, 0, 0, 0)
     *   col1 = (Ry, Uy, -Fy, 0) = (0, -1, 0, 0)
     *   col2 = (Rz, Uz, -Fz, 0) = (0, 0, -1, 0)
     *   col3 = (-dot(R,eye), -dot(U,eye), dot(F,eye), 1)
     *        = (-w/2, h/2, -cam_dist, 1)
     */
    float V[16];
    memset(V, 0, sizeof(V));
    V[0]  = 1.0f;                    /* col0.x */
    V[5]  = -1.0f;                   /* col1.y (Y flip) */
    V[10] = -1.0f;                   /* col2.z */
    V[12] = -(w * 0.5f);            /* col3.x: -dot(R, eye) */
    V[13] = h * 0.5f;               /* col3.y: -dot(U, eye), U=(0,-1,0) so -(-h/2) = h/2 */
    V[14] = -cam_dist;              /* col3.z: dot(F, eye), but F points at -Z, so -(cam_dist) */
    V[15] = 1.0f;

    /* ── Multiply P × V → out (column-major) ────────────── */
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += P[k * 4 + r] * V[c * 4 + k];
            }
            out[c * 4 + r] = sum;
        }
    }
}

void renderer_init(void) {
    /* Unit quad (0,0) to (1,1) */
    float quad_verts[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };

    glGenVertexArrays(1, &s_renderer.quad_vao);
    glGenBuffers(1, &s_renderer.quad_vbo);

    glBindVertexArray(s_renderer.quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_renderer.quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    /* Compile shaders */
    s_renderer.ui_shader = shader_compile(s_ui_vert_src, s_ui_frag_src);
    s_renderer.ui_tex_shader = shader_compile(s_ui_tex_vert_src, s_ui_tex_frag_src);
    s_renderer.voxel_shader = shader_compile(s_voxel_vert_src, s_voxel_frag_src);
    s_renderer.outline_shader = shader_compile(s_outline_vert_src, s_outline_frag_src);

    if (!s_renderer.ui_shader || !s_renderer.ui_tex_shader) {
        LOG_ERROR("Failed to compile UI shaders");
    }
    if (!s_renderer.outline_shader) {
        LOG_ERROR("Failed to compile outline shader");
    } else {
        /* Set texture unit bindings and default edge detection parameters */
        shader_use(s_renderer.outline_shader);
        shader_set_int(s_renderer.outline_shader, "u_color_tex", 0);
        shader_set_int(s_renderer.outline_shader, "u_normal_tex", 1);
        shader_set_int(s_renderer.outline_shader, "u_depth_tex", 2);
        shader_set_float(s_renderer.outline_shader, "u_depth_threshold", 0.003f);
        shader_set_float(s_renderer.outline_shader, "u_normal_threshold", 0.4f);
        shader_set_float(s_renderer.outline_shader, "u_near", 0.1f);
        shader_set_float(s_renderer.outline_shader, "u_far", 100.0f);
        shader_set_int(s_renderer.outline_shader, "u_ortho", 0);
        glUseProgram(0);
    }
    /* Empty VAO for fullscreen triangle (vertex ID trick, no buffer needed) */
    glGenVertexArrays(1, &s_renderer.outline_dummy_vao);

    if (!s_renderer.voxel_shader) {
        LOG_ERROR("Failed to compile voxel shader");
    } else {
        /* Default AO uniforms */
        shader_use(s_renderer.voxel_shader);
        shader_set_float(s_renderer.voxel_shader, "u_ao_intensity", 1.0f);
        shader_set_int(s_renderer.voxel_shader, "u_ao_texture", 0);
        shader_set_int(s_renderer.voxel_shader, "u_floor_lightmap", 1);
        shader_set_vec4(s_renderer.voxel_shader, "u_lightmap_bounds", 0.0f, 0.0f, 1.0f, 1.0f);
        /* Default deformation uniforms (identity — no deformation) */
        shader_set_float(s_renderer.voxel_shader, "u_deform_squash", 1.0f);
        shader_set_float(s_renderer.voxel_shader, "u_deform_flare", 0.0f);
        shader_set_vec2(s_renderer.voxel_shader, "u_deform_lean", 0.0f, 0.0f);
        shader_set_vec2(s_renderer.voxel_shader, "u_deform_squish_dir", 0.0f, 0.0f);
        shader_set_float(s_renderer.voxel_shader, "u_deform_squish", 0.0f);
        shader_set_float(s_renderer.voxel_shader, "u_deform_height", 0.0f);
        /* Cel-shading off by default */
        shader_set_int(s_renderer.voxel_shader, "u_cel_shading", 0);
        /* Actor ground-proximity AO (world-space) — disabled by default */
        shader_set_float(s_renderer.voxel_shader, "u_actor_ground_y", 0.0f);
        shader_set_float(s_renderer.voxel_shader, "u_actor_height", 0.0f);
        /* Conveyor belt scroll — no animation by default */
        shader_set_float(s_renderer.voxel_shader, "u_conveyor_scroll", 0.0f);
        shader_set_vec2(s_renderer.voxel_shader, "u_conveyor_dir", 1.0f, 0.0f);
        /* Default wall stone uniforms (weathered stone) */
        shader_set_vec4(s_renderer.voxel_shader, "u_wall_stone_a", 0.09f, 0.15f, 0.30f, 0.06f);
        shader_set_vec4(s_renderer.voxel_shader, "u_wall_stone_b", 0.50f, 0.22f, 0.12f, 0.20f);
        shader_set_vec4(s_renderer.voxel_shader, "u_wall_stone_c", 0.10f, 10.0f, 0.0f, 0.0f);
        shader_set_vec4(s_renderer.voxel_shader, "u_wall_stone_d", 0.0f, 0.0f, 0.0f, 0.0f);
        glUseProgram(0);
    }

    /* Create a 1×1 white fallback texture so samplers are never unbound.
     * This silences the macOS "unit 0 GLD_TEXTURE_INDEX_2D is unloadable"
     * warning that fires when a sampler is read before any texture is bound. */
    {
        GLubyte white = 255;
        glGenTextures(1, &s_renderer.fallback_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_renderer.fallback_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0,
                     GL_RED, GL_UNSIGNED_BYTE, &white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        /* Leave it bound — it acts as the default for unit 0 */

        /* Also bind fallback to unit 1 for floor lightmap sampler */
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_renderer.fallback_tex);
        glActiveTexture(GL_TEXTURE0);
    }

    LOG_INFO("Renderer initialized");
}

void renderer_shutdown(void) {
    if (s_renderer.ui_shader)     glDeleteProgram(s_renderer.ui_shader);
    if (s_renderer.ui_tex_shader) glDeleteProgram(s_renderer.ui_tex_shader);
    if (s_renderer.voxel_shader)  glDeleteProgram(s_renderer.voxel_shader);
    if (s_renderer.outline_shader) glDeleteProgram(s_renderer.outline_shader);
    if (s_renderer.quad_vao)      glDeleteVertexArrays(1, &s_renderer.quad_vao);
    if (s_renderer.quad_vbo)      glDeleteBuffers(1, &s_renderer.quad_vbo);
    if (s_renderer.outline_dummy_vao) glDeleteVertexArrays(1, &s_renderer.outline_dummy_vao);
    if (s_renderer.fallback_tex)  glDeleteTextures(1, &s_renderer.fallback_tex);
    if (s_renderer.outline_fbo)       glDeleteFramebuffers(1, &s_renderer.outline_fbo);
    if (s_renderer.outline_color_tex)  glDeleteTextures(1, &s_renderer.outline_color_tex);
    if (s_renderer.outline_normal_tex) glDeleteTextures(1, &s_renderer.outline_normal_tex);
    if (s_renderer.outline_depth_tex)  glDeleteTextures(1, &s_renderer.outline_depth_tex);
    memset(&s_renderer, 0, sizeof(s_renderer));
    LOG_INFO("Renderer shut down");
}

void renderer_begin_frame(void) {
    int w = g_engine.window_width;
    int h = g_engine.window_height;

    glViewport(0, 0, w, h);

    /* Build projection matrix based on camera mode */
    if (g_settings.camera_perspective && h > 0) {
        build_perspective_view(s_renderer.projection, (float)w, (float)h,
                               g_settings.camera_fov);
    } else {
        /* Ortho: screen-space pixels, origin at top-left */
        build_ortho(s_renderer.projection, 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    }

    /*
     * Depth test stays disabled for now — all current geometry is at z=0,
     * so GL_LESS would reject everything after the first draw per pixel.
     * Enable depth test later when we have 3D geometry with varying Z.
     */
    glDisable(GL_DEPTH_TEST);

    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_end_frame(void) {
    /* Nothing to flush yet */
}

void renderer_clear(Color color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_get_viewport(int* w, int* h) {
    *w = g_engine.window_width;
    *h = g_engine.window_height;
}

GLuint renderer_get_ui_shader(void) {
    return s_renderer.ui_shader;
}

const float* renderer_get_projection_matrix(void) {
    return s_renderer.projection;
}

GLuint renderer_get_ui_tex_shader(void) {
    return s_renderer.ui_tex_shader;
}

GLuint renderer_get_quad_vao(void) {
    return s_renderer.quad_vao;
}

GLuint renderer_get_voxel_shader(void) {
    return s_renderer.voxel_shader;
}

/* ---------- Outline post-process pass ---------- */

void renderer_begin_outline_pass(int w, int h) {
    outline_fbo_ensure(w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, s_renderer.outline_fbo);

    /* Enable both color outputs */
    GLenum draw_bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, draw_bufs);

    glViewport(0, 0, w, h);

    /* Clear color+normal+depth+stencil. Normal buffer gets default up-facing
     * normal (encoded 0.5, 1.0, 0.5) so background areas don't trigger edge
     * detection.  Stencil is cleared to 0 for multi-plane shadow rendering. */
    glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
    glClearStencil(0);
    glStencilMask(0xFF);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void renderer_end_outline_pass(int w, int h) {
    /* Switch back to default framebuffer */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Restore single draw buffer */
    GLenum draw_buf = GL_BACK;
    glDrawBuffers(1, &draw_buf);

    glViewport(0, 0, w, h);

    /* Disable depth testing for fullscreen quad */
    glDisable(GL_DEPTH_TEST);

    /* Bind the outline shader */
    shader_use(s_renderer.outline_shader);

    /* Bind FBO textures to units 0, 1, 2 */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_renderer.outline_color_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_renderer.outline_normal_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, s_renderer.outline_depth_tex);

    /* Set texel size for edge detection sampling */
    shader_set_vec2(s_renderer.outline_shader, "u_texel_size",
                    1.0f / (float)w, 1.0f / (float)h);

    /* Set projection mode for correct depth linearization.
     * The diorama camera uses near=0.1, far=100.0 for both modes. */
    shader_set_int(s_renderer.outline_shader, "u_ortho",
                   g_settings.camera_perspective ? 0 : 1);
    shader_set_float(s_renderer.outline_shader, "u_near", 0.1f);
    shader_set_float(s_renderer.outline_shader, "u_far", 100.0f);

    /* Draw fullscreen triangle (3 verts, no buffer — uses gl_VertexID) */
    glBindVertexArray(s_renderer.outline_dummy_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    /* Restore texture unit 0 and 1 to fallback textures for subsequent draws */
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_renderer.fallback_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_renderer.fallback_tex);

    glUseProgram(0);
}
