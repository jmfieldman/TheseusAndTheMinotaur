#ifndef ENGINE_UTILS_H
#define ENGINE_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* ---------- Logging ---------- */
#define LOG_INFO(fmt, ...)  fprintf(stdout, "[INFO ] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, "[WARN ] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#ifdef NDEBUG
#define LOG_DEBUG(fmt, ...) ((void)0)
#else
#define LOG_DEBUG(fmt, ...) fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

/* ---------- Common types ---------- */
typedef struct {
    float r, g, b, a;
} Color;

#define COLOR_WHITE   ((Color){1.0f, 1.0f, 1.0f, 1.0f})
#define COLOR_BLACK   ((Color){0.0f, 0.0f, 0.0f, 1.0f})
#define COLOR_CLEAR   ((Color){0.0f, 0.0f, 0.0f, 0.0f})

static inline Color color_rgba(float r, float g, float b, float a) {
    return (Color){r, g, b, a};
}

static inline Color color_hex(uint32_t hex) {
    return (Color){
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        (hex & 0xFF) / 255.0f,
        1.0f
    };
}

/* ---------- Math helpers ---------- */
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define LERP(a, b, t)    ((a) + ((b) - (a)) * (t))
#define MIN(a, b)         ((a) < (b) ? (a) : (b))
#define MAX(a, b)         ((a) > (b) ? (a) : (b))

/* ---------- Array size ---------- */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif /* ENGINE_UTILS_H */
