#ifndef TEXT_RENDER_H
#define TEXT_RENDER_H

#include "engine/utils.h"
#include <stdbool.h>

typedef enum {
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
} TextAlign;

typedef enum {
    TEXT_SIZE_SMALL  = 16,
    TEXT_SIZE_BODY   = 24,
    TEXT_SIZE_LARGE  = 32,
    TEXT_SIZE_TITLE  = 48,
    TEXT_SIZE_HERO   = 64
} TextSize;

/* Initialize text rendering. Must be called after renderer_init(). */
bool text_render_init(const char* font_path);

/* Shutdown text rendering and free all cached textures. */
void text_render_shutdown(void);

/* Draw text at the given screen position. */
void text_render_draw(const char* text, float x, float y,
                      TextSize size, Color color, TextAlign align);

/* Measure the pixel dimensions a string would occupy. */
void text_render_measure(const char* text, TextSize size, int* out_w, int* out_h);

#endif /* TEXT_RENDER_H */
