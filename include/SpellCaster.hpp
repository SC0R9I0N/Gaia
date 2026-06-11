#pragma once

#include <vector>
#include <SDL.h>

enum class CastInput {
    Left,
    Right
};

enum class CastState {
    Idle,
    Casting
};

class SpellCaster {
public: 
        void begin();
        void appendInput(CastInput input);
        void update(float deltaSeconds);
        void render(SDL_Renderer* renderer);
        bool isCasting() const;
        void resolveSequence(int x, int y);
        void drawCircle(SDL_Renderer* renderer, int x_center, int y_center, int radius) const;
        int m_spellCast = 0;

private:
        CastState m_state = CastState::Idle;
        std::vector<CastInput> m_sequence;
        float m_castTimer = 0.0f;
        float m_castWindow = 2.0f;
        float m_spellTimer = 0.0f;
        float m_circleX = 0.0f;
        float m_circleY = 0.0f;
}; 
