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
        case AssetKind::Character2: return "assets/character_2.png";
        case AssetKind::Character3: return "assets/character_3.png";
        case AssetKind::Character4: return "assets/character_4.png";
        case AssetKind::Character5: return "assets/character_5.png";
        case AssetKind::Character6: return "assets/character_6.png";
        case AssetKind::Dash:       return "assets/dash.png";
        case AssetKind::Dash2:      return "assets/dash_2.png";
        case AssetKind::Dash3:      return "assets/dash_3.png";
        case AssetKind::Dash4:      return "assets/dash_4.png";
        case AssetKind::Dash5:      return "assets/dash_5.png";
        case AssetKind::Dash6:      return "assets/dash_6.png";
        case AssetKind::Vendor:     return "assets/vendor.png";
        case AssetKind::RuneVendor: return "assets/rune_vendor.png";
        case AssetKind::ShadyVendor: return "assets/shady_vendor.png";
        case AssetKind::ConsciousnessConsole: return "assets/consciousness_console.png";
        case AssetKind::SkillTreeConsole: return "assets/skill_tree_console.png";
        case AssetKind::ArtifactStorage: return "assets/artifact_storage.png";
        case AssetKind::RunPortal:  return "assets/run_portal.png";
        case AssetKind::GoldCurrency: return "assets/currency_gold.png";
        case AssetKind::SkillCurrency: return "assets/currency_skill.png";
        case AssetKind::ShadyCurrency: return "assets/currency_shady.png";
        case AssetKind::Enemy:      return "assets/enemy.png";
        case AssetKind::Floor:      return "assets/floor.png";
        case AssetKind::Attack:     return "assets/attack.png";
        case AssetKind::HubFloor:   return "assets/hub_floor.png";
        case AssetKind::HubWall:    return "assets/hub_wall.png";
        case AssetKind::HubLantern: return "assets/hub_lantern.png";
        case AssetKind::HubPillar:  return "assets/hub_pillar.png";
        case AssetKind::HubUrn:     return "assets/hub_urn.png";
        case AssetKind::HubBanner:  return "assets/hub_banner.png";
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
        case AssetKind::Character2: slot = &m_character2; break;
        case AssetKind::Character3: slot = &m_character3; break;
        case AssetKind::Character4: slot = &m_character4; break;
        case AssetKind::Character5: slot = &m_character5; break;
        case AssetKind::Character6: slot = &m_character6; break;
        case AssetKind::Dash:       slot = &m_dash;       break;
        case AssetKind::Dash2:      slot = &m_dash2;      break;
        case AssetKind::Dash3:      slot = &m_dash3;      break;
        case AssetKind::Dash4:      slot = &m_dash4;      break;
        case AssetKind::Dash5:      slot = &m_dash5;      break;
        case AssetKind::Dash6:      slot = &m_dash6;      break;
        case AssetKind::Item:       slot = &m_item;       break;
        case AssetKind::Background:  slot = &m_background;  break;
        case AssetKind::Vendor:     slot = &m_vendor;     break;
        case AssetKind::RuneVendor: slot = &m_runeVendor; break;
        case AssetKind::ShadyVendor: slot = &m_shadyVendor; break;
        case AssetKind::ConsciousnessConsole: slot = &m_consciousnessConsole; break;
        case AssetKind::SkillTreeConsole: slot = &m_skillTreeConsole; break;
        case AssetKind::ArtifactStorage: slot = &m_artifactStorage; break;
        case AssetKind::RunPortal:  slot = &m_runPortal; break;
        case AssetKind::GoldCurrency: slot = &m_goldCurrency; break;
        case AssetKind::SkillCurrency: slot = &m_skillCurrency; break;
        case AssetKind::ShadyCurrency: slot = &m_shadyCurrency; break;
        case AssetKind::Enemy:      slot = &m_enemy;      break;
        case AssetKind::Floor:      slot = &m_floor;      break;
        case AssetKind::Attack:     slot = &m_attack;     break;
        case AssetKind::HubFloor:   slot = &m_hubFloor;   break;
        case AssetKind::HubWall:    slot = &m_hubWall;    break;
        case AssetKind::HubLantern: slot = &m_hubLantern; break;
        case AssetKind::HubPillar:  slot = &m_hubPillar;  break;
        case AssetKind::HubUrn:     slot = &m_hubUrn;     break;
        case AssetKind::HubBanner:  slot = &m_hubBanner;  break;
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
        case AssetKind::Character2: return makeCharacter();
        case AssetKind::Character3: return makeCharacter();
        case AssetKind::Character4: return makeCharacter();
        case AssetKind::Character5: return makeCharacter();
        case AssetKind::Character6: return makeCharacter();
        case AssetKind::Dash:       return makeDash();
        case AssetKind::Dash2:      return makeDash();
        case AssetKind::Dash3:      return makeDash();
        case AssetKind::Dash4:      return makeDash();
        case AssetKind::Dash5:      return makeDash();
        case AssetKind::Dash6:      return makeDash();
        case AssetKind::Item:       return makeItem();
        case AssetKind::Background:  return makeBackground();
        case AssetKind::Vendor:     return makeVendor();
        case AssetKind::RuneVendor: return makeRuneVendor();
        case AssetKind::ShadyVendor: return makeShadyVendor();
        case AssetKind::ConsciousnessConsole: return makeConsciousnessConsole();
        case AssetKind::SkillTreeConsole: return makeSkillTreeConsole();
        case AssetKind::ArtifactStorage: return makeArtifactStorage();
        case AssetKind::RunPortal:  return makeRunPortal();
        case AssetKind::GoldCurrency: return makeGoldCurrency();
        case AssetKind::SkillCurrency: return makeSkillCurrency();
        case AssetKind::ShadyCurrency: return makeShadyCurrency();
        case AssetKind::Enemy:      return makeEnemy();
        case AssetKind::Floor:      return makeFloor();
        case AssetKind::Attack:     return makeAttack();
        // Hub tileset fallbacks: only used if the PNGs are missing, so route to
        // the closest existing stand-in rather than authoring new procedural art.
        case AssetKind::HubFloor:   return makeFloor();
        case AssetKind::HubWall:    return makeFloor();
        case AssetKind::HubLantern: return makeItem();
        case AssetKind::HubPillar:  return makeVendor();
        case AssetKind::HubUrn:     return makeItem();
        case AssetKind::HubBanner:  return makeVendor();
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

SDL_Texture* PlaceholderTextures::makeDash() {
    return makeCharacter();
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

SDL_Texture* PlaceholderTextures::makeRuneVendor() {
    return makeVendor();
}

SDL_Texture* PlaceholderTextures::makeShadyVendor() {
    return makeVendor();
}

SDL_Texture* PlaceholderTextures::makeConsciousnessConsole() {
    return makeVendor();
}

SDL_Texture* PlaceholderTextures::makeSkillTreeConsole() {
    return makeVendor();
}

SDL_Texture* PlaceholderTextures::makeArtifactStorage() {
    return makeVendor();
}

SDL_Texture* PlaceholderTextures::makeRunPortal() {
    return makeItem();
}

SDL_Texture* PlaceholderTextures::makeGoldCurrency() {
    return makeItem();
}

SDL_Texture* PlaceholderTextures::makeSkillCurrency() {
    return makeItem();
}

SDL_Texture* PlaceholderTextures::makeShadyCurrency() {
    return makeItem();
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
    if (m_character2) { SDL_DestroyTexture(m_character2); m_character2 = nullptr; }
    if (m_character3) { SDL_DestroyTexture(m_character3); m_character3 = nullptr; }
    if (m_character4) { SDL_DestroyTexture(m_character4); m_character4 = nullptr; }
    if (m_character5) { SDL_DestroyTexture(m_character5); m_character5 = nullptr; }
    if (m_character6) { SDL_DestroyTexture(m_character6); m_character6 = nullptr; }
    if (m_dash)       { SDL_DestroyTexture(m_dash);       m_dash       = nullptr; }
    if (m_dash2)      { SDL_DestroyTexture(m_dash2);      m_dash2      = nullptr; }
    if (m_dash3)      { SDL_DestroyTexture(m_dash3);      m_dash3      = nullptr; }
    if (m_dash4)      { SDL_DestroyTexture(m_dash4);      m_dash4      = nullptr; }
    if (m_dash5)      { SDL_DestroyTexture(m_dash5);      m_dash5      = nullptr; }
    if (m_dash6)      { SDL_DestroyTexture(m_dash6);      m_dash6      = nullptr; }
    if (m_item)       { SDL_DestroyTexture(m_item);       m_item       = nullptr; }
    if (m_background) { SDL_DestroyTexture(m_background); m_background = nullptr; }
    if (m_vendor)     { SDL_DestroyTexture(m_vendor);     m_vendor     = nullptr; }
    if (m_runeVendor) { SDL_DestroyTexture(m_runeVendor); m_runeVendor = nullptr; }
    if (m_shadyVendor) { SDL_DestroyTexture(m_shadyVendor); m_shadyVendor = nullptr; }
    if (m_consciousnessConsole) { SDL_DestroyTexture(m_consciousnessConsole); m_consciousnessConsole = nullptr; }
    if (m_skillTreeConsole) { SDL_DestroyTexture(m_skillTreeConsole); m_skillTreeConsole = nullptr; }
    if (m_artifactStorage) { SDL_DestroyTexture(m_artifactStorage); m_artifactStorage = nullptr; }
    if (m_runPortal)  { SDL_DestroyTexture(m_runPortal);  m_runPortal  = nullptr; }
    if (m_goldCurrency) { SDL_DestroyTexture(m_goldCurrency); m_goldCurrency = nullptr; }
    if (m_skillCurrency) { SDL_DestroyTexture(m_skillCurrency); m_skillCurrency = nullptr; }
    if (m_shadyCurrency) { SDL_DestroyTexture(m_shadyCurrency); m_shadyCurrency = nullptr; }
    if (m_enemy)      { SDL_DestroyTexture(m_enemy);      m_enemy      = nullptr; }
    if (m_floor)      { SDL_DestroyTexture(m_floor);      m_floor      = nullptr; }
    if (m_attack)     { SDL_DestroyTexture(m_attack);     m_attack     = nullptr; }
    if (m_hubFloor)   { SDL_DestroyTexture(m_hubFloor);   m_hubFloor   = nullptr; }
    if (m_hubWall)    { SDL_DestroyTexture(m_hubWall);    m_hubWall    = nullptr; }
    if (m_hubLantern) { SDL_DestroyTexture(m_hubLantern); m_hubLantern = nullptr; }
    if (m_hubPillar)  { SDL_DestroyTexture(m_hubPillar);  m_hubPillar  = nullptr; }
    if (m_hubUrn)     { SDL_DestroyTexture(m_hubUrn);     m_hubUrn     = nullptr; }
    if (m_hubBanner)  { SDL_DestroyTexture(m_hubBanner);  m_hubBanner  = nullptr; }
}

}  // namespace gaia
