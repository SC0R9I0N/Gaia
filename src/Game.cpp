#include "Game.hpp"

#include "PlaceholderTextures.hpp"
#include "Player.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>

namespace gaia {

// A few stationary item placeholders, just to show the Item texture in use.
namespace {
const SDL_Point kItemSpots[] = {
    {300, 220}, {820, 180}, {640, 520}, {980, 470}};
constexpr int kItemSize = 28;
}  // namespace

static SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path) {
    int width, height, channels;
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, 4);
    if (!pixels) return nullptr;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    stbi_image_free(pixels);

    return texture;
}

Game::Game() = default;

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

    // Build the placeholder texture set and drop the player in the middle.
    m_textures = std::make_unique<PlaceholderTextures>();
    m_textures->init(m_renderer, m_width, m_height);

    m_player = std::make_unique<Player>();
    m_player->init(m_textures.get(),
                   static_cast<float>(m_width)  * 0.5f - 24.0f,
                   static_cast<float>(m_height) * 0.5f - 24.0f);

    m_running = true;

    if (TTF_Init() == -1) {
    // TTF_GetError() for details
    }
    
    m_font = TTF_OpenFont("assets/fonts/font.ttf", 24);
    if (!m_font) {
        std::fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
        return false;
    }

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
                } else if (event.key.repeat == 0 && m_player) {
                    // One-shot keyboard actions (roll/use); WASD is polled below.
                    m_player->handleAction(event.key.keysym.sym);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                // Left mouse button swings the melee attack.
                if (event.button.button == SDL_BUTTON_LEFT && m_player) {
                    if (m_player->isCasting()) {
                        m_player->appendCastInput(CastInput::Left);
                    } else {
                        m_player->attack();
                    }
                } else if (event.button.button == SDL_BUTTON_RIGHT && m_player) {
                    if (m_player->isCasting()) {
                        m_player->appendCastInput(CastInput::Right);
                    }
                } else if (event.button.button == SDL_BUTTON_MIDDLE && m_player) {
                    if (m_player->isCasting()) {
                        m_player->castSpell();   
                    } else {
                        m_player->beginCasting();
                    }
                }
                break;
            default:
                break;
        }
    }
}

void Game::update(float deltaSeconds) {
    if (m_player) {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        m_player->update(deltaSeconds, keys, m_width, m_height);
    }
}

void Game::render() {
    // Background placeholder fills the whole window.
    if (m_textures) {
        SDL_RenderCopy(m_renderer, m_textures->defaultFor(AssetKind::Background),
                       nullptr, nullptr);
    } else {
        SDL_SetRenderDrawColor(m_renderer, 24, 26, 31, 255);
        SDL_RenderClear(m_renderer);
    }

    // Item placeholders.
    if (m_textures) {
        SDL_Texture* itemTex = m_textures->defaultFor(AssetKind::Item);
        for (const SDL_Point& p : kItemSpots) {
            SDL_Rect dst{p.x, p.y, kItemSize, kItemSize};
            SDL_RenderCopy(m_renderer, itemTex, nullptr, &dst);
        }
    }

    // The player draws itself (sprite, melee hitbox, item effect).
    if (m_player) {
        m_player->render(m_renderer);
    }

    SDL_Color color = {255, 255, 255, 255}; // RGBA
    SDL_Surface* surface = TTF_RenderText_Blended(m_font, "Fireball: < >", color);  
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface); // free immediately, you don't need it anymore
    int w, h;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
    SDL_Rect dst = { 10, 10, w, h };
    SDL_RenderCopy(m_renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture); // free after rendering
    
    SDL_RenderPresent(m_renderer);
}

void Game::shutdown() {
    // Textures must be freed before the renderer that created them.
    m_player.reset();
    m_textures.reset();

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    TTF_Quit();
    // SDL_Quit is idempotent; calling it after a partial init is fine.
    SDL_Quit();
    m_running = false;
}

}  // namespace gaia
