#pragma once

#include <SDL.h>

#include <random>
#include <vector>

#include "EnemyTypes.hpp"

namespace gaia {

class EnemySystem {
public:
    void clear();
    void spawnRandom(const SDL_Rect& room, float playerX, float playerY, int count);
    // Spawns one enemy at each given world-space point (used by run layouts that
    // hand-place their encounters). Replaces any existing enemies.
    void spawnAt(const std::vector<SDL_Point>& points);
    void update(float dt, const SDL_Rect& room, const std::vector<SDL_Rect>& holes,
                float playerX, float playerY);
    // texture is the Enemy sprite (AssetKind::Enemy); pass nullptr to fall back
    // to the hand-drawn circle.
    void render(SDL_Renderer* renderer, float cameraX, float cameraY,
                SDL_Texture* texture = nullptr) const;

    int damageInRect(const SDL_Rect& hitbox, int damage);
    int damageCircle(float x, float y, float radius, int damage,
                     float knockbackDirectionX, float knockbackDirectionY,
                     float knockbackPower, bool* hit = nullptr);
    int damagePlayer(float playerX, float playerY, float playerSize);
    bool empty() const { return m_enemies.empty(); }
    // Number of living enemies (for the HUD / cleared-room check).
    int count() const { return static_cast<int>(m_enemies.size()); }

private:
    struct Enemy {
        float x = 0.0f;
        float y = 0.0f;
        float invuln = 0.0f;
        float abilityCooldown = 0.0f;
        float chargeTimer = 0.0f;
        float chargeDirX = 1.0f;
        float chargeDirY = 0.0f;
        float aiPhase = 0.0f;
        int health = 1;        // set from the type's health on spawn
        int typeIndex = 0;     // index into enemyTypeRegistry()
    };

    struct Projectile {
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float radius = 7.0f;
        float ttl = 0.0f;
        int damage = 1;
        SDL_Color color{255, 255, 255, 255};
    };

    struct AreaEffect {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 0.0f;
        float ttl = 0.0f;
        float duration = 0.0f;
        int damage = 1;
        SDL_Color color{255, 255, 255, 255};
    };

    static constexpr float kHitCooldown = 0.18f;

    // The archetype this enemy was spawned as. Stats (radius, speed, health,
    // colour, ability) all come from here.
    static const EnemyType& typeOf(const Enemy& enemy) {
        return enemyTypeRegistry()[static_cast<std::size_t>(enemy.typeIndex)];
    }

    // Builds an enemy of a randomly chosen archetype, positioned at (x, y).
    Enemy makeRandomEnemy(float x, float y);
    Enemy makeEnemyOfClass(EnemyClass cls, float x, float y);

    bool enemyIntersectsRect(const Enemy& enemy, const SDL_Rect& rect) const;
    bool enemyIntersectsCircle(const Enemy& enemy, float x, float y, float radius) const;

    std::vector<Enemy> m_enemies;
    std::vector<Projectile> m_projectiles;
    std::vector<AreaEffect> m_areaEffects;
    std::mt19937 m_rng{std::random_device{}()};
};

}  // namespace gaia
