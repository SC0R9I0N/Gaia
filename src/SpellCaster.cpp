#include "SpellCaster.hpp"

#include <cmath>
#include <cstdio>
#include <SDL.h>

const std::vector<SpellDef>& spellRegistry() {
    static const std::vector<SpellDef> spells = {
        {"Fireball",  1, {CastInput::Left, CastInput::Right}, 80.0f},
        {"Lightning", 2, {CastInput::Left, CastInput::Left, CastInput::Right}, 160.0f},
    };
    return spells;
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

    if (m_spellActive) {
        // Advance toward the target; stop and vanish once it is reached.
        const float step = kSpellSpeed * deltaSeconds;
        const float dx = m_targetX - m_circleX;
        const float dy = m_targetY - m_circleY;
        const float remaining = std::sqrt(dx * dx + dy * dy);
        if (step >= remaining) {
            m_circleX = m_targetX;
            m_circleY = m_targetY;
            m_spellActive = false;
        } else {
            m_circleX += m_dirX * step;
            m_circleY += m_dirY * step;
        }
    }
}

void SpellCaster::render(SDL_Renderer* renderer, float cameraX, float cameraY) {
    if (!m_spellActive) return;

    // Every spell uses the same placeholder: a travelling red circle.
    SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    drawCircle(renderer,
               static_cast<int>(m_circleX - cameraX),
               static_cast<int>(m_circleY - cameraY),
               kSpellRadius);
}

bool SpellCaster::isCasting() const {
    return m_state == CastState::Casting;
}

bool SpellCaster::activeCircle(float* x, float* y, float* radius, float* spellDirectionX, float* spellDirectionY, float* knockbackPower) const {
    if (!m_spellActive) {
        return false;
    }
    if (x) *x = m_circleX;
    if (y) *y = m_circleY;
    if (radius) *radius = static_cast<float>(kSpellRadius);
    if (spellDirectionX) *spellDirectionX = m_dirX;
    if (spellDirectionY) *spellDirectionY = m_dirY;
    if (knockbackPower) *knockbackPower = m_knockbackPower;
    return true;
}

void SpellCaster::clearActiveSpell() {
    m_spellActive = false;
}

//TODO: clean up eventually so theres not so much reused code
void SpellCaster::resolveSequence(float startX, float startY, float targetX, float targetY) {
    // Match the entered sequence against the shared registry; first hit wins.
    for (const SpellDef& spell : spellRegistry()) {
        if (m_sequence == spell.sequence) {
            std::fprintf(stderr, "%s\n", spell.name);
            m_spellCast = spell.spellCast;
            m_knockbackPower = spell.knockbackPower;
            // Launch the projectile from the player toward the cursor.
            m_circleX = startX;
            m_circleY = startY;
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