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
    "out vec3 v_world_pos;\n"
    "out vec3 v_normal;\n"
    "out vec4 v_color;\n"
    "out vec2 v_uv;\n"
    "out float v_ao_mode;\n"
    "\n"
    "void main() {\n"
    "    vec4 world = u_model * vec4(a_position, 1.0);\n"
    "    v_world_pos = world.xyz;\n"
    "    v_normal = mat3(u_model) * a_normal;\n"
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
    "\n"
    "out vec4 FragColor;\n"
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
    "void main() {\n"
    "    vec3 N = normalize(v_normal);\n"
    "    vec3 base_color = v_color.rgb;\n"
    "\n"
    "    /* Three-tier AO system:\n"
    "     *   ao_mode > 1.5 → floor lightmap\n"
    "     *   ao_mode > 0.5 → raytraced AO atlas\n"
    "     *   ao_mode <= 0.5 → vertex color already darkened (wall heuristic) */\n"
    "    float ao = 1.0;\n"
    "    if (v_ao_mode > 2.5) {\n"
    "        /* Actor shadow: sample shadow texture (on unit 1) as alpha.\n"
    "         * UVs map [0,1] across the shadow texture. Output black\n"
    "         * with alpha from texture; blending darkens the floor. */\n"
    "        float shadow_val = texture(u_floor_lightmap, v_uv).r;\n"
    "        FragColor = vec4(0.0, 0.0, 0.0, 1.0 - shadow_val);\n"
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
    "    } else if (v_ao_mode > 0.5 && u_has_ao != 0) {\n"
    "        /* Raytraced AO atlas */\n"
    "        float ao_sample = texture(u_ao_texture, v_uv).r;\n"
    "        ao = mix(1.0, ao_sample, u_ao_intensity);\n"
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
    "    }\n"
    "\n"
    "    /* Apply AO to base color */\n"
    "    base_color *= ao;\n"
    "    float final_alpha = v_color.a;\n"
    "\n"
    "    /* Ambient */\n"
    "    vec3 lit = u_ambient_color.rgb * base_color;\n"
    "\n"
    "    /* Directional diffuse (half-Lambert for softer look) */\n"
    "    float ndl = dot(N, u_light_dir.xyz);\n"
    "    float diffuse = ndl * 0.5 + 0.5;\n"  /* half-Lambert */
    "    lit += u_light_color.rgb * base_color * diffuse;\n"
    "\n"
    "    /* Point lights */\n"
    "    for (int i = 0; i < u_point_count; i++) {\n"
    "        vec3 to_light = u_point_pos[i].xyz - v_world_pos;\n"
    "        float dist = length(to_light);\n"
    "        float atten = max(0.0, 1.0 - dist / u_point_radius[i]);\n"
    "        atten *= atten;\n"  /* quadratic falloff */
    "        float pndl = max(0.0, dot(N, normalize(to_light)));\n"
    "        lit += u_point_color[i].rgb * base_color * pndl * atten;\n"
    "    }\n"
    "\n"
    "    FragColor = vec4(lit, final_alpha);\n"
    "}\n";

/* ---------- State ---------- */

static struct {
    GLuint ui_shader;
    GLuint ui_tex_shader;
    GLuint voxel_shader;
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint fallback_tex;    /* 1×1 white texture — keeps samplers valid */
    float  projection[16];
} s_renderer;

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

    if (!s_renderer.ui_shader || !s_renderer.ui_tex_shader) {
        LOG_ERROR("Failed to compile UI shaders");
    }
    if (!s_renderer.voxel_shader) {
        LOG_ERROR("Failed to compile voxel shader");
    } else {
        /* Default AO uniforms */
        shader_use(s_renderer.voxel_shader);
        shader_set_float(s_renderer.voxel_shader, "u_ao_intensity", 1.0f);
        shader_set_int(s_renderer.voxel_shader, "u_ao_texture", 0);
        shader_set_int(s_renderer.voxel_shader, "u_floor_lightmap", 1);
        shader_set_vec4(s_renderer.voxel_shader, "u_lightmap_bounds", 0.0f, 0.0f, 1.0f, 1.0f);
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
    if (s_renderer.quad_vao)      glDeleteVertexArrays(1, &s_renderer.quad_vao);
    if (s_renderer.quad_vbo)      glDeleteBuffers(1, &s_renderer.quad_vbo);
    if (s_renderer.fallback_tex)  glDeleteTextures(1, &s_renderer.fallback_tex);
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
