#pragma once

#include <SDL.h>

#include <random>
#include <vector>

namespace gaia {

class EnemySystem {
public:
    void clear();
    void spawnRandom(const SDL_Rect& room, float playerX, float playerY, int count);
    void update(float dt, const SDL_Rect& room, float playerX, float playerY);
    void render(SDL_Renderer* renderer, float cameraX, float cameraY) const;

    void damageInRect(const SDL_Rect& hitbox, int damage);
    bool damageCircle(float x, float y, float radius, int damage);
    bool empty() const { return m_enemies.empty(); }

private:
    struct Enemy {
        float x = 0.0f;
        float y = 0.0f;
        float invuln = 0.0f;
        int health = 3;
    };

    static constexpr float kRadius = 22.0f;
    static constexpr float kSpeed = 95.0f;
    static constexpr float kHitCooldown = 0.18f;

    bool enemyIntersectsRect(const Enemy& enemy, const SDL_Rect& rect) const;
    bool enemyIntersectsCircle(const Enemy& enemy, float x, float y, float radius) const;

    std::vector<Enemy> m_enemies;
    std::mt19937 m_rng{std::random_device{}()};
};

}  // namespace gaia
