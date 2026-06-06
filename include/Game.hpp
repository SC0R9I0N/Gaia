#pragma once

#include <cstdint>

// Forward declarations keep SDL out of the public header.
struct SDL_Window;
struct SDL_Renderer;

namespace gaia {

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
    Game() = default;
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

    // A small piece of state to prove the update/render loop is alive:
    // a square that bounces around the screen.
    float m_boxX  = 100.0f;
    float m_boxY  = 100.0f;
    float m_velX  = 220.0f;  // pixels per second
    float m_velY  = 180.0f;
    const float m_boxSize = 64.0f;
};

}  // namespace gaia
