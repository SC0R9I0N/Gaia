#include "SpellCaster.hpp"

#include <cstdio>
#include <SDL.h>

void SpellCaster::begin() {
    m_state = CastState::Casting;
    m_castTimer = m_castWindow;
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

    if (m_spellTimer > 0.0f) {
        m_spellTimer -= deltaSeconds;
        m_circleX += 200.0f * deltaSeconds;
    }
}

void SpellCaster::render(SDL_Renderer* renderer) {
    if (m_spellTimer <= 0.0f) return;

    SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    drawCircle(renderer,
                static_cast<int>(m_circleX),
                static_cast<int>(m_circleY),
                40);
}

bool SpellCaster::isCasting() const {
    return m_state == CastState::Casting;
}

void SpellCaster::resolveSequence(int x, int y) {
    m_circleX = x;
    m_circleY = y;
    if (m_sequence == std::vector<CastInput>{CastInput::Left, CastInput::Right}) {
        std::fprintf(stderr, "Fireball");
        m_spellTimer = 5.0f;
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