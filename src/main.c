#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <glad/gl.h>

#include "engine/engine.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (!engine_init("Theseus and the Minotaur", 1280, 720)) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    engine_run();
    engine_shutdown();

    return 0;
}
