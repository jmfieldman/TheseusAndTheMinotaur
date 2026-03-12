#include "platform/platform.h"
#include "engine/utils.h"

#include <SDL3/SDL.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static char s_save_dir[512] = {0};
static char s_asset_dir[512] = {0};

void platform_init(void) {
    /* Save directory */
#if defined(__APPLE__)
    #if TARGET_OS_IPHONE || TARGET_OS_TV
        /* iOS/tvOS: use app sandbox Documents directory */
        const char* base = SDL_GetPrefPath("com.theseusandtheminotaur", "TheseusAndTheMinotaur");
        if (base) {
            snprintf(s_save_dir, sizeof(s_save_dir), "%s", base);
        }
    #else
        /* macOS */
        const char* home = SDL_GetPrefPath("com.theseusandtheminotaur", "TheseusAndTheMinotaur");
        if (home) {
            snprintf(s_save_dir, sizeof(s_save_dir), "%s", home);
        }
    #endif
#elif defined(_WIN32)
    const char* base = SDL_GetPrefPath("TheseusAndTheMinotaur", "TheseusAndTheMinotaur");
    if (base) {
        snprintf(s_save_dir, sizeof(s_save_dir), "%s", base);
    }
#else
    /* Linux */
    const char* base = SDL_GetPrefPath("theseusandtheminotaur", "TheseusAndTheMinotaur");
    if (base) {
        snprintf(s_save_dir, sizeof(s_save_dir), "%s", base);
    }
#endif

    /* Ensure save dir exists */
    platform_ensure_dir(s_save_dir);

    /* Asset directory: relative to executable */
    const char* base_path = SDL_GetBasePath();
    if (base_path) {
        snprintf(s_asset_dir, sizeof(s_asset_dir), "%s", base_path);
        /* Remove trailing slash if present */
        size_t len = strlen(s_asset_dir);
        if (len > 0 && (s_asset_dir[len-1] == '/' || s_asset_dir[len-1] == '\\')) {
            s_asset_dir[len-1] = '\0';
        }
    } else {
        snprintf(s_asset_dir, sizeof(s_asset_dir), ".");
    }

    LOG_INFO("Save dir: %s", s_save_dir);
    LOG_INFO("Asset dir: %s", s_asset_dir);
}

const char* platform_get_save_dir(void) {
    return s_save_dir;
}

const char* platform_get_asset_dir(void) {
    return s_asset_dir;
}

bool platform_ensure_dir(const char* path) {
    if (!path || !path[0]) return false;

#ifdef _WIN32
    /* Windows: use _mkdir */
    struct _stat st;
    if (_stat(path, &st) == 0) return true;
    return _mkdir(path) == 0;
#else
    struct stat st;
    if (stat(path, &st) == 0) return true;
    return mkdir(path, 0755) == 0;
#endif
}

bool platform_is_mobile(void) {
#if TARGET_OS_IPHONE
    return true;
#else
    return false;
#endif
}

bool platform_show_quit(void) {
#if TARGET_OS_IPHONE || TARGET_OS_TV
    return false;
#else
    return true;
#endif
}
