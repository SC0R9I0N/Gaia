cmake_minimum_required(VERSION 3.16)
project(Gaia VERSION 0.1.0 LANGUAGES CXX)

# ---- Project-wide settings -------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Put all binaries in build/bin so assets can be found with a predictable path.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---- Dependencies ----------------------------------------------------------
# SDL2 is fetched and built automatically, so no system-wide install is needed.
include(FetchContent)

# Build SDL2 as a static lib so the final executable is self-contained.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
    SDL2_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-2.22.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2_ttf)

# ---- Game executable -------------------------------------------------------
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Game.cpp
    src/Player.cpp
    src/PlaceholderTextures.cpp
    src/SpellCaster.cpp
    src/Room.cpp
    src/Enemy.cpp
    src/EnemyTypes.cpp
)

if(WIN32)
    enable_language(RC)
    target_sources(${PROJECT_NAME} PRIVATE resources/app_icon.rc)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2_ttf::SDL2_ttf-static)

# ---- Asset handling --------------------------------------------------------
# Copy the assets folder next to the executable after every build so the game
# can load resources via a relative "assets/..." path.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets to output directory"
)
