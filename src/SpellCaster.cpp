#include "SpellCaster.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <SDL.h>

#include "stb_image.h"

const std::vector<SpellDef>& allSpellRegistry() {
    static const std::vector<SpellDef> spells = {
        {"Fireball", "Balanced explosive bolt.",
         1, {CastInput::Left, CastInput::Right}, SpellMotion::Straight,
         SDL_Color{255, 100, 30, 255}, 2, 40.0f, 40.0f, 600.0f, 1.7f, 80.0f, false},
        {"Lightning", "Fast piercing shock line.",
         2, {CastInput::Left, CastInput::Left, CastInput::Right}, SpellMotion::Straight,
         SDL_Color{90, 220, 255, 255}, 3, 24.0f, 24.0f, 1100.0f, 1.1f, 160.0f, true},
        {"Frostbolt", "Slow, compact frost projectile.",
         3, {CastInput::Right, CastInput::Left}, SpellMotion::Straight,
         SDL_Color{130, 210, 255, 255}, 1, 32.0f, 32.0f, 420.0f, 2.2f, 40.0f, false},
        {"Arcane Lance", "Thin bolt that accelerates and pierces.",
         4, {CastInput::Right, CastInput::Right}, SpellMotion::Accelerating,
         SDL_Color{190, 95, 255, 255}, 4, 18.0f, 18.0f, 470.0f, 1.4f, 20.0f, true},
        {"Ember Wave", "Swaying flame that snakes through enemies.",
         5, {CastInput::Left, CastInput::Left, CastInput::Left}, SpellMotion::Wave,
         SDL_Color{255, 145, 55, 255}, 2, 30.0f, 30.0f, 620.0f, 1.8f, 90.0f, true},
        {"Stone Comet", "Heavy, slow impact with huge knockback.",
         6, {CastInput::Left, CastInput::Right, CastInput::Right}, SpellMotion::Straight,
         SDL_Color{150, 112, 76, 255}, 4, 52.0f, 52.0f, 330.0f, 2.4f, 220.0f, false},
        {"Void Orb", "Dark orb that returns after flying out.",
         7, {CastInput::Right, CastInput::Left, CastInput::Left}, SpellMotion::Boomerang,
         SDL_Color{120, 70, 210, 255}, 2, 36.0f, 36.0f, 480.0f, 1.55f, 140.0f, true},
        {"Chain Spark", "Small spiral spark with piercing hits.",
         8, {CastInput::Right, CastInput::Left, CastInput::Right}, SpellMotion::Spiral,
         SDL_Color{120, 250, 230, 255}, 2, 24.0f, 24.0f, 760.0f, 1.5f, 70.0f, true},
        {"Solar Flare", "Growing sunburst projectile.",
         9, {CastInput::Left, CastInput::Left, CastInput::Right, CastInput::Right}, SpellMotion::Expanding,
         SDL_Color{255, 220, 90, 255}, 5, 24.0f, 70.0f, 380.0f, 1.8f, 180.0f, false},
        {"Ice Bloom", "Expanding icy bloom that travels slowly.",
         10, {CastInput::Right, CastInput::Right, CastInput::Left, CastInput::Left}, SpellMotion::Expanding,
         SDL_Color{170, 235, 255, 255}, 3, 20.0f, 64.0f, 250.0f, 2.2f, 80.0f, true},
        {"Thunder Hook", "Boomerang arc with high speed.",
         11, {CastInput::Left, CastInput::Right, CastInput::Right, CastInput::Left}, SpellMotion::Boomerang,
         SDL_Color{220, 240, 80, 255}, 3, 26.0f, 26.0f, 760.0f, 1.25f, 130.0f, true},
        {"Meteor Seed", "Accelerating heavy ember that hits hard.",
         12, {CastInput::Right, CastInput::Left, CastInput::Left, CastInput::Right}, SpellMotion::Accelerating,
         SDL_Color{255, 75, 45, 255}, 6, 34.0f, 34.0f, 360.0f, 1.9f, 240.0f, false},
        {"Gale Cutter", "Wide wind arc that weaves through targets.",
         13, {CastInput::Left, CastInput::Right, CastInput::Left, CastInput::Left}, SpellMotion::Wave,
         SDL_Color{180, 255, 210, 255}, 3, 38.0f, 38.0f, 700.0f, 1.6f, 110.0f, true},
        {"Rune Star", "Spiraling star with a long active trail.",
         14, {CastInput::Right, CastInput::Right, CastInput::Right, CastInput::Left}, SpellMotion::Spiral,
         SDL_Color{255, 180, 245, 255}, 4, 28.0f, 28.0f, 640.0f, 2.0f, 95.0f, true},
        {"Blood Moon", "Large expanding crimson orb.",
         15, {CastInput::Left, CastInput::Right, CastInput::Left, CastInput::Right, CastInput::Left}, SpellMotion::Expanding,
         SDL_Color{210, 40, 90, 255}, 7, 30.0f, 82.0f, 300.0f, 2.1f, 210.0f, false},
        {"Mirror Bolt", "Quick returning mirror-magic shot.",
         16, {CastInput::Right, CastInput::Left, CastInput::Right, CastInput::Right, CastInput::Left}, SpellMotion::Boomerang,
         SDL_Color{210, 230, 255, 255}, 4, 22.0f, 22.0f, 880.0f, 1.15f, 75.0f, true},
        {"Prismatic Drill", "Tiny bolt that ramps into a piercing drill.",
         17, {CastInput::Left, CastInput::Left, CastInput::Right, CastInput::Left, CastInput::Right}, SpellMotion::Accelerating,
         SDL_Color{120, 255, 150, 255}, 5, 16.0f, 16.0f, 520.0f, 1.7f, 55.0f, true},
    };
    return spells;
}

namespace {
SDL_Texture* loadPngTexture(SDL_Renderer* renderer, const char* path) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
    if (!pixels) {
        return nullptr;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    SDL_Texture* texture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : nullptr;
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    SDL_FreeSurface(surface);
    stbi_image_free(pixels);
    return texture;
}

SDL_Texture* spellSheet(SDL_Renderer* renderer, int spellCast, const char* suffix) {
    static SDL_Renderer* cachedRenderer = nullptr;
    static std::array<SDL_Texture*, 18> projectile{};
    static std::array<SDL_Texture*, 18> enemy{};
    static std::array<SDL_Texture*, 18> wall{};
    if (cachedRenderer != renderer) {
        projectile.fill(nullptr);
        enemy.fill(nullptr);
        wall.fill(nullptr);
        cachedRenderer = renderer;
    }
    if (spellCast < 1 || spellCast >= static_cast<int>(projectile.size())) {
        return nullptr;
    }

    std::array<SDL_Texture*, 18>* bank = &projectile;
    if (std::string(suffix) == "enemy") {
        bank = &enemy;
    } else if (std::string(suffix) == "wall") {
        bank = &wall;
    }
    SDL_Texture*& texture = (*bank)[static_cast<std::size_t>(spellCast)];
    if (!texture) {
        char path[96]{};
        std::snprintf(path, sizeof(path), "assets/spell_%02d_%s.png", spellCast, suffix);
        texture = loadPngTexture(renderer, path);
    }
    return texture;
}

SDL_Rect animationFrame(SDL_Texture* texture, float elapsed, float duration) {
    int sheetW = 0;
    int sheetH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &sheetW, &sheetH);
    const int frameSize = sheetH > 0 ? sheetH : 1;
    const int frameCount = std::max(1, sheetW / frameSize);
    const float t = duration > 0.0f ? std::clamp(elapsed / duration, 0.0f, 0.999f) : 0.0f;
    const int frame = std::clamp(static_cast<int>(t * frameCount), 0, frameCount - 1);
    return SDL_Rect{frame * frameSize, 0, frameSize, frameSize};
}

void drawFallbackCircle(SDL_Renderer* renderer, int x, int y, int radius, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 145);
    for (int yy = -radius; yy <= radius; ++yy) {
        const int halfWidth = static_cast<int>(
            std::sqrt(static_cast<float>(radius * radius - yy * yy)));
        SDL_RenderDrawLine(renderer, x - halfWidth, y + yy,
                           x + halfWidth, y + yy);
    }
}
}  // namespace

const std::vector<SpellDef>& spellRegistry() {
    static std::vector<SpellDef> loadout;
    loadout.clear();
    const std::vector<SpellDef>& all = allSpellRegistry();
    for (int index : spellLoadoutIndices()) {
        if (index >= 0 && index < static_cast<int>(all.size())) {
            loadout.push_back(all[static_cast<std::size_t>(index)]);
        }
    }
    return loadout;
}

const std::vector<int>& spellLoadoutIndices() {
    static std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    return indices;
}

bool setActiveSpellIndex(int loadoutSlot, int allSpellIndex) {
    const std::vector<SpellDef>& all = allSpellRegistry();
    if (loadoutSlot < 0 || loadoutSlot >= 10 ||
        allSpellIndex < 0 || allSpellIndex >= static_cast<int>(all.size())) {
        return false;
    }
    std::vector<int>& indices = const_cast<std::vector<int>&>(spellLoadoutIndices());
    for (int& activeIndex : indices) {
        if (activeIndex == allSpellIndex) {
            activeIndex = indices[static_cast<std::size_t>(loadoutSlot)];
            break;
        }
    }
    indices[static_cast<std::size_t>(loadoutSlot)] = allSpellIndex;
    return true;
}

const char* castInputName(CastInput input) {
    switch (input) {
        case CastInput::Left:  return "Left Click";
        case CastInput::Right: return "Right Click";
    }
    return "?";
}

void SpellCaster::begin() {
    m_state = CastState::Casting;
    m_castTimer = m_castWindow;
    m_spellCast = 0;
    m_sequence.clear();
    std::fprintf(stderr, "Casting begun\n");
}

void SpellCaster::appendInput(CastInput input) {
    if (m_state != CastState::Casting) return;

    m_sequence.push_back(input);
    m_castTimer = m_castWindow;

    std::fprintf(stderr, "Input added, sequence length %zu\n", m_sequence.size());

    //TODO check sequence against spell registry here
}

void SpellCaster::update(float deltaSeconds) {
    if (m_state == CastState::Casting) {
        m_castTimer -= deltaSeconds;
        if (m_castTimer < 0.0f) {
            m_state = CastState::Idle;
            m_sequence.clear();
            std::fprintf(stderr, "Cast timed out\n");
        }
    }

    if (m_impactActive) {
        m_impactTimer += deltaSeconds;
        if (m_impactTimer >= m_impactDuration) {
            m_impactActive = false;
        }
    }

    if (m_spellActive) {
        m_elapsed += deltaSeconds;
        if (m_elapsed >= m_lifetime) {
            clearActiveSpell(SpellImpactKind::Wall);
            return;
        }

        float speed = m_speed;
        if (m_motion == SpellMotion::Accelerating) {
            speed += 620.0f * m_elapsed;
        }
        const float step = speed * deltaSeconds;
        const float perpX = -m_dirY;
        const float perpY = m_dirX;

        switch (m_motion) {
            case SpellMotion::Wave: {
                m_distance += step;
                const float wave = std::sin(m_elapsed * 13.0f) * 44.0f;
                m_circleX = m_originX + m_dirX * m_distance + perpX * wave;
                m_circleY = m_originY + m_dirY * m_distance + perpY * wave;
                break;
            }
            case SpellMotion::Boomerang: {
                const float sign = m_elapsed < m_lifetime * 0.5f ? 1.0f : -1.0f;
                m_circleX += m_dirX * step * sign;
                m_circleY += m_dirY * step * sign;
                break;
            }
            case SpellMotion::Expanding: {
                const float t = m_elapsed / m_lifetime;
                m_radius = m_baseRadius + (m_maxRadius - m_baseRadius) * t;
                m_circleX += m_dirX * step;
                m_circleY += m_dirY * step;
                break;
            }
            case SpellMotion::Spiral: {
                m_distance += step;
                const float wave = std::sin(m_elapsed * 18.0f) * (18.0f + m_elapsed * 26.0f);
                m_circleX = m_originX + m_dirX * m_distance + perpX * wave;
                m_circleY = m_originY + m_dirY * m_distance + perpY * wave;
                break;
            }
            case SpellMotion::Straight:
            case SpellMotion::Accelerating:
                m_circleX += m_dirX * step;
                m_circleY += m_dirY * step;
                break;
        }
    }
}

void SpellCaster::render(SDL_Renderer* renderer, float cameraX, float cameraY) {
    if (m_spellActive) {
        const int x = static_cast<int>(m_circleX - cameraX);
        const int y = static_cast<int>(m_circleY - cameraY);
        const int radius = static_cast<int>(m_radius);
        if (SDL_Texture* sheet = spellSheet(renderer, m_spellCast, "projectile")) {
            const SDL_Rect src = animationFrame(sheet, m_elapsed, m_lifetime);
            const int drawSize = m_spellCast == 2
                ? std::max(96, radius * 5)
                : std::max(48, radius * 2 + 20);
            const SDL_Rect dst{x - drawSize / 2, y - drawSize / 2, drawSize, drawSize};
            const double angle = std::atan2(m_dirY, m_dirX) * 180.0 / 3.14159265358979323846;
            SDL_RenderCopyEx(renderer, sheet, &src, &dst,
                             m_spellCast == 2 ? 0.0 : angle, nullptr, SDL_FLIP_NONE);
        } else {
            drawFallbackCircle(renderer, x, y, radius, m_color);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            drawCircle(renderer, x, y, radius);
            SDL_SetRenderDrawColor(renderer, m_color.r, m_color.g, m_color.b, 180);
            drawCircle(renderer, x, y, std::max(2, radius / 2));
        }
    }

    if (m_impactActive) {
        const int x = static_cast<int>(m_impactX - cameraX);
        const int y = static_cast<int>(m_impactY - cameraY);
        const char* suffix = m_impactKind == SpellImpactKind::Enemy ? "enemy" : "wall";
        if (SDL_Texture* sheet = spellSheet(renderer, m_impactSpellCast, suffix)) {
            const SDL_Rect src = animationFrame(sheet, m_impactTimer, m_impactDuration);
            const int drawSize = std::max(72, static_cast<int>(m_impactRadius * 2.6f + 28.0f));
            const SDL_Rect dst{x - drawSize / 2, y - drawSize / 2, drawSize, drawSize};
            SDL_RenderCopy(renderer, sheet, &src, &dst);
        } else {
            const int radius = std::max(12, static_cast<int>(m_impactRadius));
            drawFallbackCircle(renderer, x, y, radius, m_color);
        }
    }
}

bool SpellCaster::isCasting() const {
    return m_state == CastState::Casting;
}

bool SpellCaster::activeCircle(float* x, float* y, float* radius,
                               float* spellDirectionX, float* spellDirectionY,
                               float* knockbackPower, int* damage,
                               bool* clearOnHit) const {
    if (!m_spellActive) {
        return false;
    }
    if (x) *x = m_circleX;
    if (y) *y = m_circleY;
    if (radius) *radius = m_radius;
    if (spellDirectionX) *spellDirectionX = m_dirX;
    if (spellDirectionY) *spellDirectionY = m_dirY;
    if (knockbackPower) *knockbackPower = m_knockbackPower;
    if (damage) *damage = m_damage;
    if (clearOnHit) *clearOnHit = !m_pierces;
    return true;
}

void SpellCaster::clearActiveSpell(SpellImpactKind impact) {
    if (m_spellActive) {
        m_impactActive = true;
        m_impactKind = impact;
        m_impactX = m_circleX;
        m_impactY = m_circleY;
        m_impactRadius = m_radius;
        m_impactTimer = 0.0f;
        m_impactSpellCast = m_spellCast;
    }
    m_spellActive = false;
}

void SpellCaster::resolveSequence(float startX, float startY, float targetX, float targetY) {
    // Match the entered sequence against the shared registry; first hit wins.
    for (const SpellDef& spell : spellRegistry()) {
        if (m_sequence == spell.sequence) {
            std::fprintf(stderr, "%s\n", spell.name);
            m_spellCast = spell.spellCast;
            m_impactActive = false;
            m_motion = spell.motion;
            m_color = spell.color;
            m_damage = spell.damage;
            m_radius = spell.radius;
            m_baseRadius = spell.radius;
            m_maxRadius = spell.maxRadius;
            m_speed = spell.speed;
            m_lifetime = spell.lifetime;
            m_elapsed = 0.0f;
            m_distance = 0.0f;
            m_pierces = spell.pierces;
            m_knockbackPower = spell.knockbackPower;
            // Launch the projectile from the player toward the cursor.
            m_circleX = startX;
            m_circleY = startY;
            m_originX = startX;
            m_originY = startY;
            m_targetX = targetX;
            m_targetY = targetY;
            const float dx = targetX - startX;
            const float dy = targetY - startY;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0001f) {
                m_dirX = dx / len;
                m_dirY = dy / len;
                m_spellActive = true;
            } else {
                // Cursor is on the player: nothing to travel to.
                m_spellActive = false;
            }
            m_castTimer = 0.0f;
            break;
        }
    }
    m_state = CastState::Idle;
    m_sequence.clear();
}

//midpoint circle drawing algorithm
void SpellCaster::drawCircle(SDL_Renderer* renderer, int x_center, int y_center, int radius) const {
    int x = radius, y = 0, err = 0;
    
    while (x >= y) {
        SDL_RenderDrawPoint(renderer, x_center + x, y_center + y);
        SDL_RenderDrawPoint(renderer, x_center + y, y_center + x);
        SDL_RenderDrawPoint(renderer, x_center - y, y_center + x);
        SDL_RenderDrawPoint(renderer, x_center - x, y_center + y);
        SDL_RenderDrawPoint(renderer, x_center - x, y_center - y);
        SDL_RenderDrawPoint(renderer, x_center - y, y_center - x);
        SDL_RenderDrawPoint(renderer, x_center + y, y_center - x);
        SDL_RenderDrawPoint(renderer, x_center + x, y_center - y);

        y++;
        err += 2 * y + 1;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}