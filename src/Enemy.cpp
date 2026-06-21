#include "Enemy.hpp"

#include <algorithm>
#include <cmath>

namespace gaia {

void EnemySystem::clear() {
    m_enemies.clear();
}

// Rolls a random archetype and returns an enemy of that type at (x, y).
EnemySystem::Enemy EnemySystem::makeRandomEnemy(float x, float y) {
    std::uniform_int_distribution<std::size_t> pick(
        0, enemyTypeRegistry().size() - 1);
    Enemy enemy{};
    enemy.typeIndex = static_cast<int>(pick(m_rng));
    enemy.health = typeOf(enemy).health;
    enemy.x = x;
    enemy.y = y;
    return enemy;
}

void EnemySystem::spawnRandom(const SDL_Rect& room, float playerX, float playerY, int count) {
    m_enemies.clear();
    const float minPlayerDistance = 280.0f;
    for (int i = 0; i < count; ++i) {
        Enemy enemy = makeRandomEnemy(0.0f, 0.0f);
        // Keep enemies fully inside the room given this type's size.
        const float r = typeOf(enemy).radius + 40.0f;
        float loX = room.x + r, hiX = room.x + room.w - r;
        float loY = room.y + r, hiY = room.y + room.h - r;
        if (hiX <= loX) loX = hiX = room.x + room.w * 0.5f;
        if (hiY <= loY) loY = hiY = room.y + room.h * 0.5f;
        std::uniform_real_distribution<float> xDist(loX, hiX);
        std::uniform_real_distribution<float> yDist(loY, hiY);
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

void EnemySystem::spawnAt(const std::vector<SDL_Point>& points) {
    m_enemies.clear();
    m_enemies.reserve(points.size());
    for (const SDL_Point& p : points) {
        // Each spawn point gets a randomly chosen enemy type.
        m_enemies.push_back(makeRandomEnemy(static_cast<float>(p.x),
                                            static_cast<float>(p.y)));
    }
}

void EnemySystem::update(float dt, const SDL_Rect& room, float playerX, float playerY) {
    for (Enemy& enemy : m_enemies) {
        const EnemyType& type = typeOf(enemy);
        if (enemy.invuln > 0.0f) {
            enemy.invuln -= dt;
        }

        // For now every class just walks toward the player at its own speed;
        // class-specific behaviour (ranged kiting, charging, etc.) comes later.
        const float dx = playerX - enemy.x;
        const float dy = playerY - enemy.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.001f) {
            enemy.x += dx / len * type.speed * dt;
            enemy.y += dy / len * type.speed * dt;
        }

        enemy.x = std::clamp(enemy.x,
                             static_cast<float>(room.x) + type.radius,
                             static_cast<float>(room.x + room.w) - type.radius);
        enemy.y = std::clamp(enemy.y,
                             static_cast<float>(room.y) + type.radius,
                             static_cast<float>(room.y + room.h) - type.radius);
    }
}

void EnemySystem::render(SDL_Renderer* renderer, float cameraX, float cameraY,
                         SDL_Texture* texture) const {
    for (const Enemy& enemy : m_enemies) {
        const EnemyType& type = typeOf(enemy);
        const int radius = static_cast<int>(type.radius);
        const int cx = static_cast<int>(enemy.x - cameraX);
        const int cy = static_cast<int>(enemy.y - cameraY);

        if (texture) {
            // Tint the shared (light) sprite with this type's colour; flash to
            // white briefly while in hit-stun for hit feedback. Size scales with
            // the type's radius.
            if (enemy.invuln > 0.0f) {
                SDL_SetTextureColorMod(texture, 255, 255, 255);
            } else {
                SDL_SetTextureColorMod(texture, type.color.r, type.color.g, type.color.b);
            }
            SDL_Rect dst{cx - radius, cy - radius, radius * 2, radius * 2};
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            continue;
        }

        // Fallback: a filled circle in the type's colour with a darker rim.
        if (enemy.invuln > 0.0f) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, type.color.r, type.color.g, type.color.b, 255);
        }
        for (int y = -radius; y <= radius; ++y) {
            const int halfWidth = static_cast<int>(
                std::sqrt(static_cast<float>(radius * radius - y * y)));
            SDL_RenderDrawLine(renderer, cx - halfWidth, cy + y,
                               cx + halfWidth, cy + y);
        }
        SDL_SetRenderDrawColor(renderer, type.color.r / 3, type.color.g / 3,
                               type.color.b / 3, 255);
        SDL_Rect bounds{cx - radius, cy - radius, radius * 2, radius * 2};
        SDL_RenderDrawRect(renderer, &bounds);
    }
    // The texture is shared/cached; leave its colour mod neutral for next frame.
    if (texture) SDL_SetTextureColorMod(texture, 255, 255, 255);
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

bool EnemySystem::damageCircle(float x, float y, float radius, int damage, float knockbackDirectionX, float knockbackDirectionY, float knockbackPower) {
    bool hit = false;
    for (Enemy& enemy : m_enemies) {
        if (enemy.invuln <= 0.0f && enemyIntersectsCircle(enemy, x, y, radius)) {
            enemy.health -= damage;
            enemy.invuln = kHitCooldown;
            // Heavier/sturdier types are shoved less.
            const float kb = knockbackPower * (1.0f - typeOf(enemy).knockbackResist);
            enemy.x += knockbackDirectionX * kb;
            enemy.y += knockbackDirectionY * kb;
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
    const float r = typeOf(enemy).radius;
    return dx * dx + dy * dy <= r * r;
}

bool EnemySystem::enemyIntersectsCircle(const Enemy& enemy, float x, float y, float radius) const {
    const float dx = enemy.x - x;
    const float dy = enemy.y - y;
    const float combined = typeOf(enemy).radius + radius;
    return dx * dx + dy * dy <= combined * combined;
}

}  // namespace gaia
