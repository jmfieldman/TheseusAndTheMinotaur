#include "engine/engine.h"
#include "engine/utils.h"
#include "input/input_manager.h"
#include "render/renderer.h"
#include "render/text_render.h"
#include "data/strings.h"
#include "data/settings.h"
#include "platform/platform.h"
#include "game/features/feature_registry.h"

#include <glad/gl.h>
#include <SDL3/SDL.h>

/* Global engine instance */
Engine g_engine = {0};

bool engine_init(const char* title, int width, int height) {
    LOG_INFO("Initializing engine...");

    /* SDL */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    /* GL attributes */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    /* Window */
    g_engine.window = SDL_CreateWindow(
        title,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!g_engine.window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    /* GL context */
    g_engine.gl_context = SDL_GL_CreateContext(g_engine.window);
    if (!g_engine.gl_context) {
        LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(g_engine.window, g_engine.gl_context);
    SDL_GL_SetSwapInterval(1); /* VSync */

    /* Load OpenGL functions via glad */
    int gl_version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (!gl_version) {
        LOG_ERROR("Failed to load OpenGL functions via glad");
        return false;
    }
    LOG_INFO("OpenGL %d.%d loaded", GLAD_VERSION_MAJOR(gl_version), GLAD_VERSION_MINOR(gl_version));
    LOG_INFO("Renderer: %s", glGetString(GL_RENDERER));

    /* Store window size */
    g_engine.window_width  = width;
    g_engine.window_height = height;

    /* Fixed timestep */
    g_engine.target_dt   = 1.0f / 60.0f;
    g_engine.last_tick   = SDL_GetPerformanceCounter();
    g_engine.accumulator = 0.0f;
    g_engine.running     = true;

    /* OpenGL defaults */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);

    /* State manager */
    state_manager_init(&g_engine.state_manager);

    /* Input */
    input_manager_init();

    /* Platform */
    platform_init();

    /* Settings */
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/settings.yml", platform_get_save_dir());
        settings_load(path);
    }

    /* Strings */
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/assets/strings/en.json", platform_get_asset_dir());
        strings_init(path);
    }

    /* Renderer */
    renderer_init();

    /* Text rendering */
    {
        char font_path[512];
        snprintf(font_path, sizeof(font_path), "%s/assets/fonts/theseus.ttf",
                 platform_get_asset_dir());
        text_render_init(font_path);
    }

    /* Feature factories (must be before any level loading) */
    feature_registry_init();

    LOG_INFO("Engine initialized successfully");
    return true;
}

void engine_run(void) {
    /* Push initial state (title screen) */
    extern State* title_scene_create(void);
    engine_push_state(title_scene_create());

    /* Reset timing baseline AFTER all initialization is complete.
     * last_tick was set early in engine_init(), but shader compilation,
     * font loading, and scene setup can take hundreds of ms.  Without
     * this reset the first frame sees a huge dt (capped to 0.25 s),
     * causing ~15 update iterations before a render or event poll,
     * which on macOS can race with window-activation timing and make
     * the app appear frozen on rare occasions. */
    g_engine.last_tick   = SDL_GetPerformanceCounter();
    g_engine.accumulator = 0.0f;

    while (g_engine.running) {
        /* Delta time */
        uint64_t now  = SDL_GetPerformanceCounter();
        float elapsed = (float)(now - g_engine.last_tick) /
                        (float)SDL_GetPerformanceFrequency();
        g_engine.last_tick = now;

        /* Cap large deltas (e.g. after breakpoint) */
        if (elapsed > 0.25f) elapsed = 0.25f;

        g_engine.accumulator += elapsed;

        /* --- Process input --- */
        input_manager_poll();

        SemanticAction action;
        while ((action = input_manager_next_action()) != ACTION_NONE) {
            state_manager_handle_action(&g_engine.state_manager, action);
        }

        /* --- Fixed timestep update --- */
        while (g_engine.accumulator >= g_engine.target_dt) {
            state_manager_update(&g_engine.state_manager, g_engine.target_dt);
            g_engine.accumulator -= g_engine.target_dt;
        }

        /* --- Render --- */
        /* Update window size (may have changed from resize) */
        SDL_GetWindowSizeInPixels(g_engine.window,
                                  &g_engine.window_width,
                                  &g_engine.window_height);

        renderer_begin_frame();
        state_manager_render(&g_engine.state_manager);
        renderer_end_frame();

        SDL_GL_SwapWindow(g_engine.window);

        /* Exit if state stack is empty */
        if (state_manager_empty(&g_engine.state_manager)) {
            g_engine.running = false;
        }
    }
}

void engine_shutdown(void) {
    LOG_INFO("Shutting down engine...");

    state_manager_shutdown(&g_engine.state_manager);
    text_render_shutdown();
    renderer_shutdown();
    strings_shutdown();

    if (g_engine.gl_context) {
        SDL_GL_DestroyContext(g_engine.gl_context);
    }
    if (g_engine.window) {
        SDL_DestroyWindow(g_engine.window);
    }
    SDL_Quit();

    LOG_INFO("Engine shut down");
}

void engine_quit(void) {
    g_engine.running = false;
}

void engine_push_state(State* state) {
    state_manager_push(&g_engine.state_manager, state);
}

void engine_pop_state(void) {
    state_manager_pop(&g_engine.state_manager);
}

void engine_swap_state(State* state) {
    state_manager_swap(&g_engine.state_manager, state);
}
