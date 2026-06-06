// SDL must be included in the translation unit that defines main: it
// #defines `main` to `SDL_main` so SDL2main's platform entry point (WinMain on
// Windows) can hand off to us. Without this include the link fails with an
// "undefined reference to SDL_main" error.
#include <SDL.h>

#include "Game.hpp"

// Because of the SDL_main remap above, the signature must be the standard
// `int main(int, char**)`.
int main(int /*argc*/, char* /*argv*/[]) {
    gaia::Game game;

    if (!game.init("Gaia", 1280, 720)) {
        return 1;
    }

    game.run();
    return 0;
}
