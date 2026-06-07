#pragma once

#include <vector>

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
        bool isCasting() const;
        void resolveSequence();

private:
        CastState m_state = CastState::Idle;
        std::vector<CastInput> m_sequence;
        float m_castTimer = 0.0f;
        float m_castWindow = 2.0f;
};
