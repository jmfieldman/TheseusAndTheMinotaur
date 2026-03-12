cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(theseus C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force everything static
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# --- FetchContent dependencies ---
include(FetchContent)

# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

# SDL3_ttf
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL3TTF_SAMPLES OFF CACHE BOOL "" FORCE)

# cJSON
FetchContent_Declare(
    cJSON
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.18
    GIT_SHALLOW    TRUE
)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "" FORCE)

# libyaml
FetchContent_Declare(
    libyaml
    GIT_REPOSITORY https://github.com/yaml/libyaml.git
    GIT_TAG        0.2.5
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(SDL3 SDL3_ttf cJSON libyaml)

# --- Source files ---
file(GLOB_RECURSE SOURCES
    src/*.c
)

# --- glad (committed in third_party) ---
set(GLAD_SOURCES third_party/glad/src/gl.c)

# --- Executable ---
add_executable(theseus ${SOURCES} ${GLAD_SOURCES})

target_include_directories(theseus PRIVATE
    src
    third_party/glad/include
    ${cjson_SOURCE_DIR}
    ${libyaml_SOURCE_DIR}/include
)

target_link_libraries(theseus PRIVATE
    SDL3::SDL3-static
    SDL3_ttf::SDL3_ttf
    cjson
    yaml
)

# macOS framework dependencies
if(APPLE)
    target_link_libraries(theseus PRIVATE
        "-framework OpenGL"
    )
endif()

# Linux GL
if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(theseus PRIVATE OpenGL::GL)
endif()

# --- Copy assets to build directory ---
add_custom_command(TARGET theseus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:theseus>/assets
    COMMENT "Copying assets to build directory"
)
