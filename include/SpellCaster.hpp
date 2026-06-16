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

// A castable spell: its display name, the id render()/m_spellCast uses, and the
// ordered input sequence the player must enter between beginning and releasing
// a cast. This is the single source of truth shared by the cast resolver and
// the Spellbook UI, so the two never drift apart.
struct SpellDef {
    const char* name;
    int spellCast;
    std::vector<CastInput> sequence;
    float knockbackPower;
};

// The spells the player currently has access to.
const std::vector<SpellDef>& spellRegistry();

// Human-readable label for a single cast input, e.g. "Left Click".
const char* castInputName(CastInput input);

class SpellCaster {
public: 
        void begin();
        void appendInput(CastInput input);
        void update(float deltaSeconds);
        void render(SDL_Renderer* renderer, float cameraX, float cameraY);
        bool isCasting() const;
        bool activeCircle(float* x, float* y, float* radius, float* spellDirectionX, float* spellDirectionY, float* knockbackPower) const;
        void clearActiveSpell();
        // Resolve the entered sequence. The projectile spawns at (startX, startY)
        // and travels to the cursor at (targetX, targetY), then vanishes.
        void resolveSequence(float startX, float startY, float targetX, float targetY);
        void drawCircle(SDL_Renderer* renderer, int x_center, int y_center, int radius) const;
        int m_spellCast = 0;

private:
        static constexpr float kSpellSpeed  = 600.0f;  // px/s projectile travel
        static constexpr int   kSpellRadius = 40;

        CastState m_state = CastState::Idle;
        std::vector<CastInput> m_sequence;
        float m_castTimer = 0.0f;
        float m_castWindow = 2.0f;

        // Active projectile: a red circle moving from its spawn toward the cursor.
        bool  m_spellActive = false;
        float m_circleX = 0.0f;
        float m_circleY = 0.0f;
        float m_targetX = 0.0f;
        float m_targetY = 0.0f;
        float m_dirX = 0.0f;
        float m_dirY = 0.0f;

        float m_knockbackPower = 0.0f;
};
