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

enum class SpellMotion {
    Straight,
    Wave,
    Accelerating,
    Boomerang,
    Expanding,
    Spiral
};

enum class SpellImpactKind {
    Enemy,
    Wall
};

// A castable spell: its display name, the id render()/m_spellCast uses, and the
// ordered input sequence the player must enter between beginning and releasing
// a cast. This is the single source of truth shared by the cast resolver and
// the Spellbook UI, so the two never drift apart.
struct SpellDef {
    const char* name;
    const char* description;
    int spellCast;
    std::vector<CastInput> sequence;
    SpellMotion motion;
    SDL_Color color;
    int damage;
    float radius;
    float maxRadius;
    float speed;
    float lifetime;
    float knockbackPower;
    bool pierces;
};

// All known spells, including ones outside the current player loadout.
const std::vector<SpellDef>& allSpellRegistry();
// The spells the player currently has access to; capped at 10 at a time.
const std::vector<SpellDef>& spellRegistry();
const std::vector<int>& spellLoadoutIndices();
bool setActiveSpellIndex(int loadoutSlot, int allSpellIndex);

// Human-readable label for a single cast input, e.g. "Left Click".
const char* castInputName(CastInput input);

class SpellCaster {
public: 
        void begin();
        void appendInput(CastInput input);
        void update(float deltaSeconds);
        void render(SDL_Renderer* renderer, float cameraX, float cameraY);
        bool isCasting() const;
        bool activeCircle(float* x, float* y, float* radius,
                          float* spellDirectionX, float* spellDirectionY,
                          float* knockbackPower, int* damage,
                          bool* clearOnHit) const;
        void clearActiveSpell(SpellImpactKind impact = SpellImpactKind::Wall);
        // The inputs entered so far in the in-progress cast (empty when idle).
        // Used by the HUD hotbar to indicate which spells are still reachable.
        const std::vector<CastInput>& currentSequence() const { return m_sequence; }
        // Seconds left in the current cast window (0 when not casting), and the
        // full window length, so the HUD can show a countdown.
        float castTimeRemaining() const {
            return m_state == CastState::Casting ? m_castTimer : 0.0f;
        }
        float castWindow() const { return m_castWindow; }
        // Resolve the entered sequence. The projectile spawns at (startX, startY)
        // and travels to the cursor at (targetX, targetY), then vanishes.
        void resolveSequence(float startX, float startY, float targetX, float targetY);
        void drawCircle(SDL_Renderer* renderer, int x_center, int y_center, int radius) const;
        int m_spellCast = 0;

        //getters + setters
        float getSpellX() const { return m_circleX; }
        float getSpellY() const { return m_circleY; }
        void setSpellX(float updatedSpellX) { m_circleX = updatedSpellX; }
        void setSpellY(float updatedSpellY) { m_circleY = updatedSpellY; }
        float spellSize() const { return m_radius; }

private:
        CastState m_state = CastState::Idle;
        std::vector<CastInput> m_sequence;
        float m_castTimer = 0.0f;
        float m_castWindow = 10.0f;

        // Active spell projectile/effect, configured from SpellDef on cast.
        bool  m_spellActive = false;
        float m_circleX = 0.0f;
        float m_circleY = 0.0f;
        float m_originX = 0.0f;
        float m_originY = 0.0f;
        float m_targetX = 0.0f;
        float m_targetY = 0.0f;
        float m_dirX = 0.0f;
        float m_dirY = 0.0f;
        float m_distance = 0.0f;
        float m_elapsed = 0.0f;
        float m_lifetime = 0.0f;
        float m_speed = 0.0f;
        float m_radius = 0.0f;
        float m_baseRadius = 0.0f;
        float m_maxRadius = 0.0f;

        float m_knockbackPower = 0.0f;
        int m_damage = 0;
        bool m_pierces = false;
        SpellMotion m_motion = SpellMotion::Straight;
        SDL_Color m_color{255, 100, 0, 255};

        bool m_impactActive = false;
        SpellImpactKind m_impactKind = SpellImpactKind::Wall;
        float m_impactX = 0.0f;
        float m_impactY = 0.0f;
        float m_impactRadius = 0.0f;
        float m_impactTimer = 0.0f;
        float m_impactDuration = 0.38f;
        int m_impactSpellCast = 0;
};
