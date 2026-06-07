#include "SpellCaster.hpp"

#include <cstdio>

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
    if (m_state != CastState::Casting) return;

    m_castTimer -= deltaSeconds;
    if (m_castTimer < 0.0f) {
        m_state = CastState::Idle;
        m_sequence.clear();
        std::fprintf(stderr, "Cast timed out\n");
    }
}

bool SpellCaster::isCasting() const {
    return m_state == CastState::Casting;
}

void SpellCaster::resolveSequence() {
    if (m_sequence == std::vector<CastInput>{CastInput::Left, CastInput::Right}) {
        std::fprintf(stderr, "Fireball");
    }
    m_state == CastState::Idle;
    m_sequence.clear();
}