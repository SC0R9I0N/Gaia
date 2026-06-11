#pragma once

#include <cstdint>
#include <memory>
#include <SDL_ttf.h>

// Forward declarations keep SDL out of the public header.
struct SDL_Window;
struct SDL_Renderer;

namespace gaia {

class Player;
class PlaceholderTextures;

// Owns the window, the render loop, and the lifetime of the game.
//
// Typical usage:
//     gaia::Game game;
//     if (game.init("Gaia", 1280, 720)) {
//         game.run();
//     }
// (run() calls shutdown() automatically when the loop ends.)
class Game {
public:
    Game();
    ~Game();

    // Non-copyable: it owns raw SDL resources.
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Creates the window and renderer. Returns false on failure.
    bool init(const char* title, int width, int height);

    // Runs the main loop until the window is closed. Blocks until then.
    void run();

private:
    void processEvents();
    void update(float deltaSeconds);
    void render();
    void shutdown();

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool          m_running  = false;

    int m_width  = 0;
    int m_height = 0;

    TTF_Font* m_font = nullptr;

    // Held by pointer so SDL types stay out of this header (see forward decls).
    std::unique_ptr<PlaceholderTextures> m_textures;
    std::unique_ptr<Player>              m_player;
};

}  // namespace gaia
