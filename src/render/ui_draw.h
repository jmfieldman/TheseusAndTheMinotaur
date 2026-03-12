#ifndef UI_DRAW_H
#define UI_DRAW_H

#include "engine/utils.h"

/* Draw a filled rectangle (screen-space pixels, origin top-left). */
void ui_draw_rect(float x, float y, float w, float h, Color color);

/* Draw a filled rectangle with rounded corners. */
void ui_draw_rect_rounded(float x, float y, float w, float h,
                          float radius, Color color);

#endif /* UI_DRAW_H */
