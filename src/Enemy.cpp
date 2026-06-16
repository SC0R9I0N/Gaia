#include "Enemy.hpp"

#include <algorithm>
#include <cmath>

namespace gaia {

void EnemySystem::clear() {
    m_enemies.clear();
}

void EnemySystem::spawnRandom(const SDL_Rect& room, float playerX, float playerY, int count) {
    m_enemies.clear();
    const int minX = room.x + static_cast<int>(kRadius) + 40;
    const int maxX = room.x + room.w - static_cast<int>(kRadius) - 40;
    const int minY = room.y + static_cast<int>(kRadius) + 40;
    const int maxY = room.y + room.h - static_cast<int>(kRadius) - 40;
    if (minX >= maxX || minY >= maxY) {
        return;
    }

    std::uniform_real_distribution<float> xDist(static_cast<float>(minX),
                                                static_cast<float>(maxX));
    std::uniform_real_distribution<float> yDist(static_cast<float>(minY),
                                                static_cast<float>(maxY));

    const float minPlayerDistance = 280.0f;
    for (int i = 0; i < count; ++i) {
        Enemy enemy{};
        for (int attempt = 0; attempt < 64; ++attempt) {
            enemy.x = xDist(m_rng);
            enemy.y = yDist(m_rng);
            const float dx = enemy.x - playerX;
            const float dy = enemy.y - playerY;
            if (dx * dx + dy * dy >= minPlayerDistance * minPlayerDistance) {
                break;
            }
        }
        m_enemies.push_back(enemy);
    }
}

void EnemySystem::update(float dt, const SDL_Rect& room, float playerX, float playerY) {
    for (Enemy& enemy : m_enemies) {
        if (enemy.invuln > 0.0f) {
            enemy.invuln -= dt;
        }

        const float dx = playerX - enemy.x;
        const float dy = playerY - enemy.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.001f) {
            enemy.x += dx / len * kSpeed * dt;
            enemy.y += dy / len * kSpeed * dt;
        }

        enemy.x = std::clamp(enemy.x,
                             static_cast<float>(room.x) + kRadius,
                             static_cast<float>(room.x + room.w) - kRadius);
        enemy.y = std::clamp(enemy.y,
                             static_cast<float>(room.y) + kRadius,
                             static_cast<float>(room.y + room.h) - kRadius);
    }
}

void EnemySystem::render(SDL_Renderer* renderer, float cameraX, float cameraY) const {
    for (const Enemy& enemy : m_enemies) {
        const int cx = static_cast<int>(enemy.x - cameraX);
        const int cy = static_cast<int>(enemy.y - cameraY);
        const int radius = static_cast<int>(kRadius);
        SDL_SetRenderDrawColor(renderer,
                               enemy.invuln > 0.0f ? 255 : 210,
                               45,
                               45,
                               255);
        for (int y = -radius; y <= radius; ++y) {
            const int halfWidth = static_cast<int>(
                std::sqrt(static_cast<float>(radius * radius - y * y)));
            SDL_RenderDrawLine(renderer, cx - halfWidth, cy + y,
                               cx + halfWidth, cy + y);
        }
        SDL_SetRenderDrawColor(renderer, 80, 10, 10, 255);
        SDL_Rect bounds{cx - radius, cy - radius, radius * 2, radius * 2};
        SDL_RenderDrawRect(renderer, &bounds);
    }
}

void EnemySystem::damageInRect(const SDL_Rect& hitbox, int damage) {
    for (Enemy& enemy : m_enemies) {
        if (enemy.invuln <= 0.0f && enemyIntersectsRect(enemy, hitbox)) {
            enemy.health -= damage;
            enemy.invuln = kHitCooldown;
        }
    }
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
                       [](const Enemy& enemy) { return enemy.health <= 0; }),
        m_enemies.end());
}

bool EnemySystem::damageCircle(float x, float y, float radius, int damage) {
    bool hit = false;
    for (Enemy& enemy : m_enemies) {
        if (enemy.invuln <= 0.0f && enemyIntersectsCircle(enemy, x, y, radius)) {
            enemy.health -= damage;
            enemy.invuln = kHitCooldown;
            hit = true;
        }
    }
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
                       [](const Enemy& enemy) { return enemy.health <= 0; }),
        m_enemies.end());
    return hit;
}

bool EnemySystem::enemyIntersectsRect(const Enemy& enemy, const SDL_Rect& rect) const {
    const float closestX = std::clamp(enemy.x, static_cast<float>(rect.x),
                                      static_cast<float>(rect.x + rect.w));
    const float closestY = std::clamp(enemy.y, static_cast<float>(rect.y),
                                      static_cast<float>(rect.y + rect.h));
    const float dx = enemy.x - closestX;
    const float dy = enemy.y - closestY;
    return dx * dx + dy * dy <= kRadius * kRadius;
}

bool EnemySystem::enemyIntersectsCircle(const Enemy& enemy, float x, float y, float radius) const {
    const float dx = enemy.x - x;
    const float dy = enemy.y - y;
    const float combined = kRadius + radius;
    return dx * dx + dy * dy <= combined * combined;
}

}  // namespace gaia
