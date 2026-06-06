#include "Game.hpp"

#include <SDL.h>

#include <cstdio>

namespace gaia {

Game::~Game() {
    // Safe to call even if init() failed or run() already cleaned up.
    shutdown();
}

bool Game::init(const char* title, int width, int height) {
    m_width  = width;
    m_height = height;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    m_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN);
    if (!m_window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // -1 picks the first driver that supports the requested flags.
    m_renderer = SDL_CreateRenderer(
        m_window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    m_running = true;
    return true;
}

void Game::run() {
    // Fixed reference for delta-time so movement is frame-rate independent.
    Uint64 previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (m_running) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const float delta = static_cast<float>((now - previous) / frequency);
        previous = now;

        processEvents();
        update(delta);
        render();
    }

    shutdown();
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    m_running = false;
                }
                break;
            default:
                break;
        }
    }
}

void Game::update(float deltaSeconds) {
    // Move the box and bounce it off the window edges.
    m_boxX += m_velX * deltaSeconds;
    m_boxY += m_velY * deltaSeconds;

    if (m_boxX <= 0.0f) {
        m_boxX = 0.0f;
        m_velX = -m_velX;
    } else if (m_boxX + m_boxSize >= static_cast<float>(m_width)) {
        m_boxX = static_cast<float>(m_width) - m_boxSize;
        m_velX = -m_velX;
    }

    if (m_boxY <= 0.0f) {
        m_boxY = 0.0f;
        m_velY = -m_velY;
    } else if (m_boxY + m_boxSize >= static_cast<float>(m_height)) {
        m_boxY = static_cast<float>(m_height) - m_boxSize;
        m_velY = -m_velY;
    }
}

void Game::render() {
    // Clear to a dark slate background.
    SDL_SetRenderDrawColor(m_renderer, 24, 26, 31, 255);
    SDL_RenderClear(m_renderer);

    // Draw the bouncing box.
    SDL_Rect box{
        static_cast<int>(m_boxX),
        static_cast<int>(m_boxY),
        static_cast<int>(m_boxSize),
        static_cast<int>(m_boxSize)};
    SDL_SetRenderDrawColor(m_renderer, 86, 182, 194, 255);
    SDL_RenderFillRect(m_renderer, &box);

    SDL_RenderPresent(m_renderer);
}

void Game::shutdown() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    // SDL_Quit is idempotent; calling it after a partial init is fine.
    SDL_Quit();
    m_running = false;
}

}  // namespace gaia
