#include "render/text_render.h"
#include "render/renderer.h"
#include "render/shader.h"
#include "engine/utils.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <glad/gl.h>
#include <string.h>

/* ---------- Font sizes ---------- */
#define FONT_SIZE_COUNT 5
static const int s_font_sizes[FONT_SIZE_COUNT] = { 16, 24, 32, 48, 64 };

/* ---------- Text cache ---------- */
#define TEXT_CACHE_SIZE 128

typedef struct {
    char     text[128];
    int      size;
    GLuint   texture;
    int      width;
    int      height;
    uint64_t last_used;  /* frame counter for LRU */
} TextCacheEntry;

static struct {
    TTF_Font*       fonts[FONT_SIZE_COUNT];
    TextCacheEntry  cache[TEXT_CACHE_SIZE];
    uint64_t        frame_counter;
    bool            initialized;
} s_text;

/* ---------- Helpers ---------- */

static int font_index_for_size(TextSize size) {
    for (int i = 0; i < FONT_SIZE_COUNT; i++) {
        if (s_font_sizes[i] == (int)size) return i;
    }
    return 1; /* default to body */
}

static TextCacheEntry* cache_find(const char* text, int size) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        TextCacheEntry* e = &s_text.cache[i];
        if (e->texture && e->size == size && strcmp(e->text, text) == 0) {
            e->last_used = s_text.frame_counter;
            return e;
        }
    }
    return NULL;
}

static TextCacheEntry* cache_alloc(void) {
    /* Find empty slot */
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (s_text.cache[i].texture == 0) return &s_text.cache[i];
    }
    /* Evict LRU */
    TextCacheEntry* oldest = &s_text.cache[0];
    for (int i = 1; i < TEXT_CACHE_SIZE; i++) {
        if (s_text.cache[i].last_used < oldest->last_used) {
            oldest = &s_text.cache[i];
        }
    }
    if (oldest->texture) {
        glDeleteTextures(1, &oldest->texture);
        oldest->texture = 0;
    }
    return oldest;
}

static TextCacheEntry* cache_get_or_create(const char* text, TextSize size) {
    TextCacheEntry* entry = cache_find(text, (int)size);
    if (entry) return entry;

    int fi = font_index_for_size(size);
    TTF_Font* font = s_text.fonts[fi];
    if (!font) return NULL;

    /* Render text to SDL surface */
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, 0, white);
    if (!surface) {
        LOG_ERROR("TTF_RenderText_Blended failed: %s", SDL_GetError());
        return NULL;
    }

    /* Upload to GL texture (single-channel: we use the red channel) */
    /* First convert to a format we can use */
    /* TTF_RenderText_Blended gives us ARGB8888 surface */
    int w = surface->w;
    int h = surface->h;

    /* Create RGBA texture from surface */
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Convert surface to RGBA and extract alpha into a single-channel texture.
     * SDL_ttf blended mode gives us white text with alpha in the A channel.
     * We'll upload as GL_RED using the alpha channel. */
    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    if (converted) {
        /* Extract just the alpha channel into a separate buffer */
        uint8_t* alpha_buf = (uint8_t*)malloc(w * h);
        if (alpha_buf) {
            uint8_t* pixels = (uint8_t*)converted->pixels;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    /* ABGR8888: byte order is R, G, B, A in memory (little-endian) */
                    int idx = y * converted->pitch + x * 4;
                    alpha_buf[y * w + x] = pixels[idx + 3];
                }
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED,
                         GL_UNSIGNED_BYTE, alpha_buf);
            free(alpha_buf);
        }
        SDL_DestroySurface(converted);
    }

    SDL_DestroySurface(surface);

    /* Store in cache */
    entry = cache_alloc();
    strncpy(entry->text, text, sizeof(entry->text) - 1);
    entry->text[sizeof(entry->text) - 1] = '\0';
    entry->size = (int)size;
    entry->texture = tex;
    entry->width = w;
    entry->height = h;
    entry->last_used = s_text.frame_counter;

    return entry;
}

/* ---------- Public API ---------- */

bool text_render_init(const char* font_path) {
    if (!TTF_Init()) {
        LOG_ERROR("TTF_Init failed: %s", SDL_GetError());
        return false;
    }

    memset(&s_text, 0, sizeof(s_text));

    for (int i = 0; i < FONT_SIZE_COUNT; i++) {
        s_text.fonts[i] = TTF_OpenFont(font_path, (float)s_font_sizes[i]);
        if (!s_text.fonts[i]) {
            LOG_ERROR("Failed to load font at size %d: %s",
                      s_font_sizes[i], SDL_GetError());
            /* Continue; some sizes may work */
        }
    }

    s_text.initialized = true;
    LOG_INFO("Text rendering initialized (font: %s)", font_path);
    return true;
}

void text_render_shutdown(void) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (s_text.cache[i].texture) {
            glDeleteTextures(1, &s_text.cache[i].texture);
        }
    }
    for (int i = 0; i < FONT_SIZE_COUNT; i++) {
        if (s_text.fonts[i]) {
            TTF_CloseFont(s_text.fonts[i]);
        }
    }
    TTF_Quit();
    memset(&s_text, 0, sizeof(s_text));
    LOG_INFO("Text rendering shut down");
}

void text_render_draw(const char* text, float x, float y,
                      TextSize size, Color color, TextAlign align) {
    if (!s_text.initialized || !text || !text[0]) return;

    s_text.frame_counter++;

    TextCacheEntry* entry = cache_get_or_create(text, size);
    if (!entry || !entry->texture) return;

    /* Adjust position for alignment */
    float draw_x = x;
    float draw_y = y;

    switch (align) {
    case TEXT_ALIGN_CENTER:
        draw_x -= entry->width * 0.5f;
        break;
    case TEXT_ALIGN_RIGHT:
        draw_x -= entry->width;
        break;
    case TEXT_ALIGN_LEFT:
    default:
        break;
    }

    /* Draw textured quad */
    GLuint shader = renderer_get_ui_tex_shader();
    shader_use(shader);
    shader_set_mat4(shader, "u_projection", renderer_get_ortho_matrix());
    shader_set_vec4(shader, "u_rect", draw_x, draw_y,
                    (float)entry->width, (float)entry->height);
    shader_set_vec4(shader, "u_color", color.r, color.g, color.b, color.a);
    shader_set_int(shader, "u_texture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, entry->texture);

    glBindVertexArray(renderer_get_quad_vao());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void text_render_measure(const char* text, TextSize size, int* out_w, int* out_h) {
    if (!s_text.initialized || !text || !text[0]) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return;
    }

    int fi = font_index_for_size(size);
    TTF_Font* font = s_text.fonts[fi];
    if (!font) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return;
    }

    int w = 0, h = 0;
    TTF_GetStringSize(font, text, 0, &w, &h);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}
