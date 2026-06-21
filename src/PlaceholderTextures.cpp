#include "PlaceholderTextures.hpp"

#include <cmath>    // std::sqrt
#include <cstdlib>  // std::abs

// stb_image's implementation is compiled in Game.cpp; here we only need the
// declarations so we can decode artist-supplied PNGs.
#include "stb_image.h"

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

// The single source of truth mapping each kind to the PNG an artist can drop
// into assets/ to override it. Returns nullptr for kinds with no file mapping
// (those only ever use their procedural stand-in).
const char* PlaceholderTextures::assetFileName(AssetKind kind) {
    switch (kind) {
        case AssetKind::Character:  return "assets/player.png";
        case AssetKind::Vendor:     return "assets/vendor.png";
        case AssetKind::Enemy:      return "assets/enemy.png";
        case AssetKind::Floor:      return "assets/floor.png";
        case AssetKind::Attack:     return "assets/attack.png";
        case AssetKind::Item:       return "assets/item.png";
        case AssetKind::Background:  return nullptr;  // sized to the window; generated
    }
    return nullptr;
}

SDL_Texture* PlaceholderTextures::loadFromFile(const char* path) {
    if (!path) return nullptr;
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
    if (!pixels) return nullptr;  // file missing/unreadable: caller falls back

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    SDL_Texture* tex = surface
        ? SDL_CreateTextureFromSurface(m_renderer, surface)
        : nullptr;
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    SDL_FreeSurface(surface);
    stbi_image_free(pixels);
    return tex;
}

SDL_Texture* PlaceholderTextures::defaultFor(AssetKind kind) {
    if (!m_renderer) {
        return nullptr;
    }

    // Pick the cache slot for this kind.
    SDL_Texture** slot = nullptr;
    switch (kind) {
        case AssetKind::Character:  slot = &m_character;  break;
        case AssetKind::Item:       slot = &m_item;       break;
        case AssetKind::Background:  slot = &m_background;  break;
        case AssetKind::Vendor:     slot = &m_vendor;     break;
        case AssetKind::Enemy:      slot = &m_enemy;      break;
        case AssetKind::Floor:      slot = &m_floor;      break;
        case AssetKind::Attack:     slot = &m_attack;     break;
    }
    if (!slot) return nullptr;

    if (!*slot) {
        // Prefer the artist-supplied PNG; fall back to the procedural stand-in
        // so the game always has something to draw.
        *slot = loadFromFile(assetFileName(kind));
        if (!*slot) *slot = makeProcedural(kind);
    }
    return *slot;
}

SDL_Texture* PlaceholderTextures::makeProcedural(AssetKind kind) {
    switch (kind) {
        case AssetKind::Character:  return makeCharacter();
        case AssetKind::Item:       return makeItem();
        case AssetKind::Background:  return makeBackground();
        case AssetKind::Vendor:     return makeVendor();
        case AssetKind::Enemy:      return makeEnemy();
        case AssetKind::Floor:      return makeFloor();
        case AssetKind::Attack:     return makeAttack();
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

// A simple 64x80 shopkeeper/stall marker for hub vendors.
SDL_Texture* PlaceholderTextures::makeVendor() {
    const int w = 64;
    const int h = 80;
    SDL_Texture* tex = makeTarget(m_renderer, w, h);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_renderer);

    const SDL_Rect awning{4, 4, w - 8, 18};
    SDL_SetRenderDrawColor(m_renderer, 180, 70, 95, 255);
    SDL_RenderFillRect(m_renderer, &awning);
    SDL_SetRenderDrawColor(m_renderer, 245, 220, 120, 255);
    for (int x = 8; x < w - 8; x += 16) {
        SDL_Rect stripe{x, 4, 8, 18};
        SDL_RenderFillRect(m_renderer, &stripe);
    }

    const SDL_Rect counter{8, 50, w - 16, 18};
    SDL_SetRenderDrawColor(m_renderer, 94, 62, 36, 255);
    SDL_RenderFillRect(m_renderer, &counter);
    SDL_SetRenderDrawColor(m_renderer, 142, 96, 52, 255);
    SDL_RenderDrawRect(m_renderer, &counter);

    const SDL_Rect body{20, 24, 24, 30};
    SDL_SetRenderDrawColor(m_renderer, 86, 130, 210, 255);
    SDL_RenderFillRect(m_renderer, &body);
    SDL_SetRenderDrawColor(m_renderer, 240, 200, 160, 255);
    SDL_Rect head{22, 18, 20, 18};
    SDL_RenderFillRect(m_renderer, &head);

    SDL_SetRenderDrawColor(m_renderer, 30, 34, 44, 255);
    SDL_RenderDrawRect(m_renderer, &awning);
    SDL_RenderDrawRect(m_renderer, &body);
    SDL_RenderDrawRect(m_renderer, &head);

    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

// A 44x44 creature: a filled circle with a darker rim and two eyes. Authored
// LIGHT/neutral on purpose so EnemySystem can tint it to each enemy type's
// colour with a colour multiply (a red base would muddy the tints).
SDL_Texture* PlaceholderTextures::makeEnemy() {
    const int s = 44;
    SDL_Texture* tex = makeTarget(m_renderer, s, s);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_renderer);

    const int center = s / 2;
    const int radius = center - 2;
    SDL_SetRenderDrawColor(m_renderer, 226, 228, 234, 255);  // light body
    for (int y = -radius; y <= radius; ++y) {
        const int half = static_cast<int>(
            std::sqrt(static_cast<float>(radius * radius - y * y)));
        SDL_RenderDrawLine(m_renderer, center - half, center + y,
                           center + half, center + y);
    }
    // Darker rim.
    SDL_SetRenderDrawColor(m_renderer, 150, 152, 164, 255);
    SDL_Rect bounds{center - radius, center - radius, radius * 2, radius * 2};
    SDL_RenderDrawRect(m_renderer, &bounds);
    // Eyes (dark, so they stay readable under any tint).
    SDL_SetRenderDrawColor(m_renderer, 40, 42, 54, 255);
    SDL_Rect le{center - 12, center - 6, 8, 8};
    SDL_Rect re{center + 4, center - 6, 8, 8};
    SDL_RenderFillRect(m_renderer, &le);
    SDL_RenderFillRect(m_renderer, &re);

    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

// A 32x32 seamless floor tile: a dark base with a lighter grout line on the top
// and left edges, so repeating it reads as a tiled grid.
SDL_Texture* PlaceholderTextures::makeFloor() {
    const int s = 32;
    SDL_Texture* tex = makeTarget(m_renderer, s, s);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);
    SDL_SetRenderDrawColor(m_renderer, 46, 46, 58, 255);
    SDL_RenderClear(m_renderer);

    SDL_SetRenderDrawColor(m_renderer, 58, 58, 72, 255);
    SDL_RenderDrawLine(m_renderer, 0, 0, s - 1, 0);  // top grout
    SDL_RenderDrawLine(m_renderer, 0, 0, 0, s - 1);  // left grout

    SDL_SetRenderDrawColor(m_renderer, 34, 34, 44, 255);
    SDL_Rect specks[3] = {{8, 20, 2, 2}, {22, 10, 2, 2}, {15, 15, 2, 2}};
    for (const SDL_Rect& r : specks) SDL_RenderFillRect(m_renderer, &r);

    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

// A 40x40 melee slash: a crescent of orange pixels on the right (+x) side, so
// render code can rotate it to the swing direction.
SDL_Texture* PlaceholderTextures::makeAttack() {
    const int s = 40;
    SDL_Texture* tex = makeTarget(m_renderer, s, s);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_renderer);

    // Crescent = the area inside an outer circle but outside an inner circle
    // shifted left, keeping only the right-hand arc.
    const float cx = s * 0.5f;
    const float cy = s * 0.5f;
    const float outer = s * 0.46f;
    const float inner = s * 0.40f;
    const float shift = s * 0.16f;  // pushes the cut-out left -> arc on the right
    SDL_SetRenderDrawColor(m_renderer, 255, 130, 50, 235);
    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const float dxO = x - cx,        dyO = y - cy;
            const float dxI = x - (cx - shift), dyI = y - cy;
            const bool inOuter = dxO * dxO + dyO * dyO <= outer * outer;
            const bool inInner = dxI * dxI + dyI * dyI <= inner * inner;
            if (inOuter && !inInner && x >= cx - shift) {
                SDL_RenderDrawPoint(m_renderer, x, y);
            }
        }
    }

    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

void PlaceholderTextures::destroy() {
    if (m_character)  { SDL_DestroyTexture(m_character);  m_character  = nullptr; }
    if (m_item)       { SDL_DestroyTexture(m_item);       m_item       = nullptr; }
    if (m_background) { SDL_DestroyTexture(m_background); m_background = nullptr; }
    if (m_vendor)     { SDL_DestroyTexture(m_vendor);     m_vendor     = nullptr; }
    if (m_enemy)      { SDL_DestroyTexture(m_enemy);      m_enemy      = nullptr; }
    if (m_floor)      { SDL_DestroyTexture(m_floor);      m_floor      = nullptr; }
    if (m_attack)     { SDL_DestroyTexture(m_attack);     m_attack     = nullptr; }
}

}  // namespace gaia
