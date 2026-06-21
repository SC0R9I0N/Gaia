#include "Player.hpp"

#include "Keybindings.hpp"
#include "PlaceholderTextures.hpp"

#include <cmath>
#include <cstdio>

namespace gaia {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

void Player::init(PlaceholderTextures* textures, float x, float y) {
    m_textures = textures;
    m_x = x;
    m_y = y;
}

void Player::roll() {
    if (m_state == State::Rolling || m_rollCooldownTimer > 0.0f) {
        return;
    }
    // Dodge in whichever direction we are currently facing.
    m_rollDirX = m_facingX;
    m_rollDirY = m_facingY;
    m_state = State::Rolling;
    m_rollTimer = kRollDuration;
    m_rollCooldownTimer = kRollCooldown;
    m_invulnTimer = kRollDuration + kInvulnBuffer;
}

void Player::attack(float targetX, float targetY) {
    // A roll can't be interrupted by a swing; otherwise honor the cooldown.
    if (m_state == State::Rolling || m_attackCooldownTimer > 0.0f) {
        return;
    }
    m_state = State::Attacking;
    m_attackTimer = kAttackDuration;
    m_attackCooldownTimer = kAttackCooldown;

    // Aim the swing toward the cursor rather than the movement-facing direction.
    const float dx = targetX - (m_x + kSize * 0.5f);
    const float dy = targetY - (m_y + kSize * 0.5f);
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.0001f) {
        m_attackDirX = dx / len;
        m_attackDirY = dy / len;
    } else {
        // Cursor is on the player: fall back to the current facing.
        m_attackDirX = m_facingX;
        m_attackDirY = m_facingY;
    }
}

void Player::useItem() {
    if (m_itemTimer > 0.0f) {
        return;  // effect already running; doubles as a simple cooldown
    }
    m_itemTimer = kItemDuration;
    // Placeholder effect: a brief shield (extends i-frames) plus a console log.
    // Swap this for real item logic once an inventory exists.
    if (m_invulnTimer < kItemDuration) {
        m_invulnTimer = kItemDuration;
    }
    std::printf("[item] used placeholder item -> brief shield\n");
}

void Player::update(float dt, const Uint8* keyboard, const Keybindings& binds) {
    // Tick down all timers.
    if (m_rollCooldownTimer > 0.0f)   m_rollCooldownTimer   -= dt;
    if (m_attackCooldownTimer > 0.0f) m_attackCooldownTimer -= dt;
    if (m_invulnTimer > 0.0f)         m_invulnTimer         -= dt;
    if (m_itemTimer > 0.0f)           m_itemTimer           -= dt;

    if (m_state == State::Rolling) {
        // Locked into the dodge: travel along the roll vector at burst speed.
        m_x += m_rollDirX * kRollSpeed * dt;
        m_y += m_rollDirY * kRollSpeed * dt;
        m_rollTimer -= dt;
        if (m_rollTimer <= 0.0f) {
            m_state = State::Normal;
        }
    } else {
        // Smooth WASD movement from the live keyboard state.
        float moveX = 0.0f;
        float moveY = 0.0f;
        if (binds.isHeld(Action::MoveUp,    keyboard)) moveY -= 1.0f;
        if (binds.isHeld(Action::MoveDown,  keyboard)) moveY += 1.0f;
        if (binds.isHeld(Action::MoveLeft,  keyboard)) moveX -= 1.0f;
        if (binds.isHeld(Action::MoveRight, keyboard)) moveX += 1.0f;

        // Sprite facing only ever points left or right (this isn't a fully
        // top-down view, so the texture never tilts). Horizontal input sets the
        // side; pure vertical movement leaves the previous facing untouched.
        if (moveX > 0.0f)      m_textureFacing = 1;
        else if (moveX < 0.0f) m_textureFacing = -1;

        if (moveX != 0.0f || moveY != 0.0f) {
            const float len = std::sqrt(moveX * moveX + moveY * moveY);
            moveX /= len;  // normalize so diagonals aren't faster
            moveY /= len;
            m_facingX = moveX;
            m_facingY = moveY;
            m_x += moveX * kSpeed * dt;
            m_y += moveY * kSpeed * dt;
        }

        if (m_state == State::Attacking) {
            m_attackTimer -= dt;
            if (m_attackTimer <= 0.0f) {
                m_state = State::Normal;
            }
        }
        if (m_spellTimer > 0.0f) {
            m_spellTimer -= dt;
        }
    }

    // Wall collision and door transitions are handled by the RoomSystem after
    // this update, so no clamping happens here.
    m_spellCaster.update(dt);
}

bool Player::attackHitbox(SDL_Rect* out) const {
    if (m_state != State::Attacking) {
        return false;
    }
    const float hbW   = 40.0f;
    const float hbH   = 40.0f;
    const float reach = 34.0f;
    const float cx = m_x + kSize * 0.5f + m_attackDirX * (kSize * 0.5f + reach * 0.5f);
    const float cy = m_y + kSize * 0.5f + m_attackDirY * (kSize * 0.5f + reach * 0.5f);
    out->x = static_cast<int>(cx - hbW * 0.5f);
    out->y = static_cast<int>(cy - hbH * 0.5f);
    out->w = static_cast<int>(hbW);
    out->h = static_cast<int>(hbH);
    return true;
}

void Player::render(SDL_Renderer* renderer, float cameraX, float cameraY) {
    SDL_Texture* tex = m_textures ? m_textures->defaultFor(AssetKind::Character) : nullptr;

    // Melee swing: the slash sprite, rotated to the cursor-aimed swing
    // direction and centred on the hitbox. Falls back to a translucent red
    // rectangle if no attack texture is available.
    SDL_Rect hitbox;
    if (attackHitbox(&hitbox)) {
        SDL_Texture* slash =
            m_textures ? m_textures->defaultFor(AssetKind::Attack) : nullptr;
        if (slash) {
            constexpr int kSlashSize = 64;
            const int centerX = hitbox.x + hitbox.w / 2;
            const int centerY = hitbox.y + hitbox.h / 2;
            const SDL_Rect dst{
                centerX - kSlashSize / 2 - static_cast<int>(cameraX),
                centerY - kSlashSize / 2 - static_cast<int>(cameraY),
                kSlashSize, kSlashSize};
            const double angle = std::atan2(m_attackDirY, m_attackDirX) * 180.0 / kPi;

            // The attack texture is a horizontal sprite sheet of square frames
            // (frame size == sheet height), so an artist can add frames just by
            // widening the PNG. Pick the frame for how far the swing has played.
            int sheetW = 0, sheetH = 0;
            SDL_QueryTexture(slash, nullptr, nullptr, &sheetW, &sheetH);
            const int frameSize  = sheetH > 0 ? sheetH : 1;
            const int frameCount = sheetW / frameSize > 0 ? sheetW / frameSize : 1;
            // attackTimer counts kAttackDuration -> 0, so progress runs 0 -> 1.
            const float progress = 1.0f - (m_attackTimer / kAttackDuration);
            int frame = static_cast<int>(progress * frameCount);
            if (frame < 0) frame = 0;
            if (frame >= frameCount) frame = frameCount - 1;
            const SDL_Rect src{frame * frameSize, 0, frameSize, frameSize};

            SDL_RenderCopyEx(renderer, slash, &src, &dst, angle, nullptr,
                             SDL_FLIP_NONE);
        } else {
            SDL_Rect screenHit{hitbox.x - static_cast<int>(cameraX),
                               hitbox.y - static_cast<int>(cameraY),
                               hitbox.w, hitbox.h};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 235, 90, 70, 150);
            SDL_RenderFillRect(renderer, &screenHit);
        }
    }

    // The character itself. The sprite (authored facing right) only ever flips
    // left/right to match m_textureFacing — never rotated — since this view is
    // not fully top-down. While invulnerable it blinks so the i-frames show.
    const SDL_Rect dst{
        static_cast<int>(m_x - cameraX),
        static_cast<int>(m_y - cameraY),
        static_cast<int>(kSize),
        static_cast<int>(kSize)};
    if (tex) {
        Uint8 alpha = 255;
        if (m_invulnTimer > 0.0f && (static_cast<int>(m_invulnTimer * 20.0f) % 2) == 0) {
            alpha = 110;
        }
        SDL_SetTextureAlphaMod(tex, alpha);
        const SDL_RendererFlip flip =
            m_textureFacing < 0 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_RenderCopyEx(renderer, tex, nullptr, &dst, 0.0, nullptr, flip);
        SDL_SetTextureAlphaMod(tex, 255);  // texture is shared; restore it
    } else {
        // Fallback if textures are unavailable.
        SDL_SetRenderDrawColor(renderer, 86, 182, 194, 255);
        SDL_RenderFillRect(renderer, &dst);
    }

    // Item effect: an expanding, fading green ring around the character.
    if (m_itemTimer > 0.0f) {
        const float t = 1.0f - m_itemTimer / kItemDuration;  // 0 -> 1
        const int grow = static_cast<int>(t * 40.0f);
        const SDL_Rect ring{
            static_cast<int>(m_x - cameraX) - grow,
            static_cast<int>(m_y - cameraY) - grow,
            static_cast<int>(kSize) + grow * 2,
            static_cast<int>(kSize) + grow * 2};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 120, 235, 120,
                               static_cast<Uint8>(180.0f * (1.0f - t)));
        SDL_RenderDrawRect(renderer, &ring);
    }
    m_spellCaster.render(renderer, cameraX, cameraY);
}

void Player::beginCasting() {
    m_spellCaster.begin();
}

void Player::appendCastInput(CastInput input) {
    m_spellCaster.appendInput(input);
}

bool Player::isCasting() const {
    return m_spellCaster.isCasting();
}

bool Player::activeSpellCircle(float* x, float* y, float* radius, float* spellDirectionX, float* spellDirectionY, float* knockbackPower) const {
    return m_spellCaster.activeCircle(x, y, radius, spellDirectionX, spellDirectionY, knockbackPower);
}

void Player::clearActiveSpell() {
    m_spellCaster.clearActiveSpell();
}

void Player::castSpell(float targetX, float targetY) {
    // Spawn at the player's center, travel to the cursor target.
    m_spellCaster.resolveSequence(m_x + kSize * 0.5f, m_y + kSize * 0.5f,
                                  targetX, targetY);
}

}  // namespace gaia
