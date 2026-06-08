#pragma once

#include <SDL.h>
#include "SpellCaster.hpp"

namespace gaia {

class PlaceholderTextures;

// The playable character: WASD movement plus three core actions.
//
//   - roll/dodge: a short burst of speed in the facing direction with a few
//     invulnerability frames (i-frames) on either side of it.
//   - melee attack: opens a hitbox in front of the character for a brief window.
//   - use item: fires a placeholder effect (a short shield + expanding ring).
//
// Movement reads the live keyboard state every frame for smooth motion; the
// one-shot actions are triggered from key-press events via handleAction().
class Player {
public:
    Player() = default;

    // textures must outlive the player; x/y is the starting top-left position.
    void init(PlaceholderTextures* textures, float x, float y);

    // Trigger a one-shot keyboard action from an SDL key-down event (space/E).
    void handleAction(SDL_Keycode key);

    // Swing the melee attack. Bound to the left mouse button by Game.
    void attack();

    // Advance one frame. keyboard is SDL_GetKeyboardState(); world bounds keep
    // the character on screen.
    void update(float dt, const Uint8* keyboard, int worldWidth, int worldHeight);

    void render(SDL_Renderer* renderer);

    // Exposed for future combat code (enemies, damage, etc.).
    bool isInvulnerable() const { return m_invulnTimer > 0.0f; }
    // Fills out with the active melee hitbox; returns false when not attacking.
    bool attackHitbox(SDL_Rect* out) const;
    void beginCasting();
    void appendCastInput(CastInput input);
    bool isCasting() const;
    void castSpell();

private:
    enum class State { Normal, Rolling, Attacking };

    void startRoll();
    void useItem();

    // ---- Tunables ----------------------------------------------------------
    static constexpr float kSize         = 48.0f;
    static constexpr float kSpeed        = 260.0f;  // px/s walking
    static constexpr float kRollSpeed    = 720.0f;  // px/s during a dodge
    static constexpr float kRollDuration = 0.18f;   // seconds of burst
    static constexpr float kRollCooldown = 0.55f;
    static constexpr float kInvulnBuffer = 0.06f;   // extra i-frames past the roll
    static constexpr float kAttackDuration = 0.16f; // hitbox/animation window
    static constexpr float kAttackCooldown = 0.30f;
    static constexpr float kItemDuration   = 0.50f; // placeholder effect length

    PlaceholderTextures* m_textures = nullptr;

    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_facingX = 1.0f;  // unit facing vector, starts pointing right
    float m_facingY = 0.0f;

    State m_state = State::Normal;

    float m_rollTimer        = 0.0f;
    float m_rollCooldownTimer = 0.0f;
    float m_rollDirX = 1.0f;
    float m_rollDirY = 0.0f;

    float m_invulnTimer = 0.0f;

    float m_attackTimer         = 0.0f;
    float m_attackCooldownTimer = 0.0f;

    float m_itemTimer = 0.0f;
    float m_spellTimer = 0.0f;

    SpellCaster m_spellCaster;
};

}  // namespace gaia
