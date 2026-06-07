#include "PlaceholderTextures.hpp"

#include <cstdlib>  // std::abs

namespace gaia {

namespace {

// Creates a blank, transparent texture we can render into as a target.
SDL_Texture* makeTarget(SDL_Renderer* renderer, int w, int h) {
    SDL_Texture* tex = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        w, h);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    return tex;
}

}  // namespace

PlaceholderTextures::~PlaceholderTextures() {
    destroy();
}

void PlaceholderTextures::init(SDL_Renderer* renderer, int backgroundWidth, int backgroundHeight) {
    m_renderer = renderer;
    m_bgWidth  = backgroundWidth;
    m_bgHeight = backgroundHeight;
}

SDL_Texture* PlaceholderTextures::defaultFor(AssetKind kind) {
    if (!m_renderer) {
        return nullptr;
    }
    switch (kind) {
        case AssetKind::Character:
            if (!m_character) m_character = makeCharacter();
            return m_character;
        case AssetKind::Item:
            if (!m_item) m_item = makeItem();
            return m_item;
        case AssetKind::Background:
            if (!m_background) m_background = makeBackground();
            return m_background;
    }
    return nullptr;
}

// A 48x48 teal body with a darker border and a white arrow pointing right (+x).
// The arrow lets render code rotate the sprite to show which way it faces.
SDL_Texture* PlaceholderTextures::makeCharacter() {
    const int s = 48;
    SDL_Texture* tex = makeTarget(m_renderer, s, s);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);

    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_renderer);

    SDL_Rect body{4, 4, s - 8, s - 8};
    SDL_SetRenderDrawColor(m_renderer, 86, 182, 194, 255);
    SDL_RenderFillRect(m_renderer, &body);

    SDL_SetRenderDrawColor(m_renderer, 28, 58, 64, 255);
    SDL_RenderDrawRect(m_renderer, &body);

    // Facing arrow: a filled triangle narrowing toward the right edge.
    const int baseX = s / 2;
    const int apexX = s - 8;
    const int cy    = s / 2;
    const int halfH = 9;
    SDL_SetRenderDrawColor(m_renderer, 240, 244, 245, 255);
    for (int x = baseX; x <= apexX; ++x) {
        const float t = static_cast<float>(x - baseX) / static_cast<float>(apexX - baseX);
        const int half = static_cast<int>(halfH * (1.0f - t));
        SDL_RenderDrawLine(m_renderer, x, cy - half, x, cy + half);
    }

    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

// A 28x28 gold diamond with a darker outline.
SDL_Texture* PlaceholderTextures::makeItem() {
    const int s = 28;
    SDL_Texture* tex = makeTarget(m_renderer, s, s);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);

    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_renderer);

    const int center = s / 2;
    SDL_SetRenderDrawColor(m_renderer, 232, 196, 84, 255);
    for (int y = 0; y < s; ++y) {
        const int half = center - std::abs(y - center);
        if (half > 0) {
            SDL_RenderDrawLine(m_renderer, center - half, y, center + half, y);
        }
    }

    SDL_SetRenderDrawColor(m_renderer, 120, 92, 20, 255);
    SDL_Point outline[5] = {
        {center, 0}, {s - 1, center}, {center, s - 1}, {0, center}, {center, 0}};
    SDL_RenderDrawLines(m_renderer, outline, 5);

    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

// A full-window dark backdrop with a faint grid, so movement reads as smooth
// against a fixed reference.
SDL_Texture* PlaceholderTextures::makeBackground() {
    const int w = m_bgWidth;
    const int h = m_bgHeight;
    SDL_Texture* tex = makeTarget(m_renderer, w, h);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);

    SDL_SetRenderDrawColor(m_renderer, 24, 26, 31, 255);
    SDL_RenderClear(m_renderer);

    const int cell = 64;
    SDL_SetRenderDrawColor(m_renderer, 38, 42, 50, 255);
    for (int x = 0; x <= w; x += cell) {
        SDL_RenderDrawLine(m_renderer, x, 0, x, h);
    }
    for (int y = 0; y <= h; y += cell) {
        SDL_RenderDrawLine(m_renderer, 0, y, w, y);
    }

    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

void PlaceholderTextures::destroy() {
    if (m_character)  { SDL_DestroyTexture(m_character);  m_character  = nullptr; }
    if (m_item)       { SDL_DestroyTexture(m_item);       m_item       = nullptr; }
    if (m_background) { SDL_DestroyTexture(m_background); m_background = nullptr; }
}

}  // namespace gaia
