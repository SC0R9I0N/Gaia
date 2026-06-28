#include "Enemy.hpp"

#include <algorithm>
#include <cmath>

namespace gaia {

void EnemySystem::clear() {
    m_enemies.clear();
    m_projectiles.clear();
    m_areaEffects.clear();
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
    std::uniform_real_distribution<float> phase(0.0f, 6.28318f);
    enemy.aiPhase = phase(m_rng);
    enemy.abilityCooldown = typeOf(enemy).ability.cooldown * 0.5f;
    return enemy;
}

EnemySystem::Enemy EnemySystem::makeEnemyOfClass(EnemyClass cls, float x, float y) {
    const std::vector<EnemyType>& types = enemyTypeRegistry();
    for (std::size_t i = 0; i < types.size(); ++i) {
        if (types[i].cls == cls) {
            Enemy enemy{};
            enemy.typeIndex = static_cast<int>(i);
            enemy.health = types[i].health;
            enemy.x = x;
            enemy.y = y;
            std::uniform_real_distribution<float> phase(0.0f, 6.28318f);
            enemy.aiPhase = phase(m_rng);
            enemy.abilityCooldown = types[i].ability.cooldown;
            return enemy;
        }
    }
    return makeRandomEnemy(x, y);
}

void EnemySystem::spawnRandom(const SDL_Rect& room, float playerX, float playerY, int count) {
    m_enemies.clear();
    m_projectiles.clear();
    m_areaEffects.clear();
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
    m_projectiles.clear();
    m_areaEffects.clear();
    m_enemies.reserve(points.size());
    for (const SDL_Point& p : points) {
        // Each spawn point gets a randomly chosen enemy type.
        m_enemies.push_back(makeRandomEnemy(static_cast<float>(p.x),
                                            static_cast<float>(p.y)));
    }
}

void EnemySystem::update(float dt, const SDL_Rect& room,
                         const std::vector<SDL_Rect>& holes,
                         float playerX, float playerY) {
    auto intersectsHole = [&](const Enemy& enemy) {
        const EnemyType& type = typeOf(enemy);
        const float r = type.radius;
        for (const SDL_Rect& hole : holes) {
            const float hl = static_cast<float>(hole.x);
            const float hr = static_cast<float>(hole.x + hole.w);
            const float ht = static_cast<float>(hole.y);
            const float hb = static_cast<float>(hole.y + hole.h);
            const float closestX = std::clamp(enemy.x, hl, hr);
            const float closestY = std::clamp(enemy.y, ht, hb);
            const float dx = enemy.x - closestX;
            const float dy = enemy.y - closestY;
            if (dx * dx + dy * dy <= r * r) {
                return true;
            }
        }
        return false;
    };

    auto moveEnemy = [&](Enemy& enemy, float dirX, float dirY, float speedScale = 1.0f) {
        const EnemyType& type = typeOf(enemy);
        const float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len <= 0.001f) {
            return;
        }
        dirX /= len;
        dirY /= len;
        const float oldX = enemy.x;
        const float oldY = enemy.y;
        enemy.x += dirX * type.speed * speedScale * dt;
        enemy.x = std::clamp(enemy.x,
                             static_cast<float>(room.x) + type.radius,
                             static_cast<float>(room.x + room.w) - type.radius);
        if (intersectsHole(enemy)) {
            enemy.x = oldX;
        }

        enemy.y += dirY * type.speed * speedScale * dt;
        enemy.y = std::clamp(enemy.y,
                             static_cast<float>(room.y) + type.radius,
                             static_cast<float>(room.y + room.h) - type.radius);
        if (intersectsHole(enemy)) {
            enemy.y = oldY;
        }
    };

    auto addAreaEffect = [&](float x, float y, float radius, int damage, SDL_Color color) {
        AreaEffect effect{};
        effect.x = x;
        effect.y = y;
        effect.radius = radius;
        effect.ttl = 0.35f;
        effect.duration = effect.ttl;
        effect.damage = damage;
        effect.color = color;
        m_areaEffects.push_back(effect);
    };

    auto fireProjectile = [&](const Enemy& enemy, float dirX, float dirY) {
        const float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len <= 0.001f) {
            return;
        }
        const EnemyType& type = typeOf(enemy);
        Projectile shot{};
        shot.x = enemy.x;
        shot.y = enemy.y;
        const float speed = type.cls == EnemyClass::Caster ? 280.0f : 360.0f;
        shot.vx = dirX / len * speed;
        shot.vy = dirY / len * speed;
        shot.radius = type.cls == EnemyClass::Caster ? 11.0f : 7.0f;
        shot.ttl = 2.4f;
        shot.damage = type.ability.damage > 0 ? type.ability.damage : 1;
        shot.color = type.color;
        m_projectiles.push_back(shot);
    };

    std::vector<Enemy> summons;

    for (Enemy& enemy : m_enemies) {
        const EnemyType& type = typeOf(enemy);
        if (enemy.invuln > 0.0f) {
            enemy.invuln -= dt;
        }
        if (enemy.abilityCooldown > 0.0f) {
            enemy.abilityCooldown -= dt;
        }
        enemy.aiPhase += dt;

        const float dx = playerX - enemy.x;
        const float dy = playerY - enemy.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        const float dirX = len > 0.001f ? dx / len : 1.0f;
        const float dirY = len > 0.001f ? dy / len : 0.0f;
        const float strafe = std::sin(enemy.aiPhase) >= 0.0f ? 1.0f : -1.0f;

        if (enemy.chargeTimer > 0.0f) {
            enemy.chargeTimer -= dt;
            moveEnemy(enemy, enemy.chargeDirX, enemy.chargeDirY, 3.4f);
        } else {
            switch (type.cls) {
                case EnemyClass::Ranged: {
                    const float desired = type.ability.range * 0.65f;
                    if (len < desired * 0.75f) {
                        moveEnemy(enemy, -dirX - dirY * 0.35f * strafe,
                                  -dirY + dirX * 0.35f * strafe);
                    } else if (len > desired * 1.15f) {
                        moveEnemy(enemy, dirX + -dirY * 0.25f * strafe,
                                  dirY + dirX * 0.25f * strafe);
                    } else {
                        moveEnemy(enemy, -dirY * strafe, dirX * strafe, 0.65f);
                    }
                    break;
                }
                case EnemyClass::Charger:
                    if (enemy.abilityCooldown <= 0.0f && len <= type.ability.range) {
                        enemy.chargeDirX = dirX;
                        enemy.chargeDirY = dirY;
                        enemy.chargeTimer = 0.42f;
                        enemy.abilityCooldown = type.ability.cooldown;
                    } else {
                        moveEnemy(enemy, dirX, dirY);
                    }
                    break;
                case EnemyClass::Bomber:
                    moveEnemy(enemy, dirX, dirY, 1.2f);
                    if (len <= type.ability.range) {
                        addAreaEffect(enemy.x, enemy.y, type.ability.range * 1.4f,
                                      type.ability.damage,
                                      SDL_Color{255, 220, 80, 255});
                        enemy.health = 0;
                    }
                    break;
                case EnemyClass::Tank:
                    if (len > type.ability.range * 0.75f) {
                        moveEnemy(enemy, dirX, dirY);
                    }
                    break;
                case EnemyClass::Caster: {
                    const float desired = type.ability.range * 0.75f;
                    if (len < desired * 0.75f) {
                        moveEnemy(enemy, -dirX, -dirY);
                    } else if (len > desired * 1.1f) {
                        moveEnemy(enemy, dirX, dirY, 0.75f);
                    } else {
                        moveEnemy(enemy, -dirY * strafe, dirX * strafe, 0.55f);
                    }
                    break;
                }
                case EnemyClass::Swarm:
                    moveEnemy(enemy, dirX + -dirY * 0.45f * strafe,
                              dirY + dirX * 0.45f * strafe, 1.1f);
                    break;
                case EnemyClass::Melee:
                default:
                    moveEnemy(enemy, dirX, dirY);
                    break;
            }
        }

        if (enemy.abilityCooldown <= 0.0f && len <= type.ability.range) {
            switch (type.ability.kind) {
                case EnemyAbilityKind::MeleeStrike:
                    addAreaEffect(enemy.x, enemy.y, type.ability.range,
                                  type.ability.damage,
                                  SDL_Color{255, 90, 70, 255});
                    enemy.abilityCooldown = type.ability.cooldown;
                    break;
                case EnemyAbilityKind::GroundSmash:
                    addAreaEffect(enemy.x, enemy.y, type.ability.range * 1.2f,
                                  type.ability.damage,
                                  SDL_Color{180, 190, 210, 255});
                    enemy.abilityCooldown = type.ability.cooldown;
                    break;
                case EnemyAbilityKind::RangedShot:
                case EnemyAbilityKind::ArcaneBlast:
                    fireProjectile(enemy, dx, dy);
                    enemy.abilityCooldown = type.ability.cooldown;
                    break;
                case EnemyAbilityKind::SummonMinions:
                    if (static_cast<int>(m_enemies.size() + summons.size()) < 14) {
                        summons.push_back(makeEnemyOfClass(EnemyClass::Swarm,
                                                           enemy.x - type.radius * 1.8f,
                                                           enemy.y));
                        summons.push_back(makeEnemyOfClass(EnemyClass::Swarm,
                                                           enemy.x + type.radius * 1.8f,
                                                           enemy.y));
                        addAreaEffect(enemy.x, enemy.y, type.ability.range * 0.45f,
                                      0,
                                      SDL_Color{150, 130, 255, 255});
                    }
                    enemy.abilityCooldown = type.ability.cooldown;
                    break;
                case EnemyAbilityKind::ChargeDash:
                case EnemyAbilityKind::Explode:
                case EnemyAbilityKind::None:
                    break;
            }
        }

        enemy.x = std::clamp(enemy.x,
                             static_cast<float>(room.x) + type.radius,
                             static_cast<float>(room.x + room.w) - type.radius);
        enemy.y = std::clamp(enemy.y,
                             static_cast<float>(room.y) + type.radius,
                             static_cast<float>(room.y + room.h) - type.radius);
    }

    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
                       [](const Enemy& enemy) { return enemy.health <= 0; }),
        m_enemies.end());
    m_enemies.insert(m_enemies.end(), summons.begin(), summons.end());

    for (Projectile& shot : m_projectiles) {
        shot.x += shot.vx * dt;
        shot.y += shot.vy * dt;
        shot.ttl -= dt;
        const bool outOfRoom =
            shot.x < room.x || shot.x > room.x + room.w ||
            shot.y < room.y || shot.y > room.y + room.h;
        if (outOfRoom) {
            shot.ttl = 0.0f;
        }
        for (const SDL_Rect& hole : holes) {
            if (shot.x > hole.x && shot.x < hole.x + hole.w &&
                shot.y > hole.y && shot.y < hole.y + hole.h) {
                shot.ttl = 0.0f;
            }
        }
    }
    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                       [](const Projectile& shot) { return shot.ttl <= 0.0f; }),
        m_projectiles.end());

    for (AreaEffect& effect : m_areaEffects) {
        effect.ttl -= dt;
    }
    m_areaEffects.erase(
        std::remove_if(m_areaEffects.begin(), m_areaEffects.end(),
                       [](const AreaEffect& effect) { return effect.ttl <= 0.0f; }),
        m_areaEffects.end());
}

void EnemySystem::render(SDL_Renderer* renderer, float cameraX, float cameraY,
                         SDL_Texture* texture) const {
    auto drawFilledCircle = [&](int cx, int cy, int radius, SDL_Color color) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        for (int y = -radius; y <= radius; ++y) {
            const int halfWidth = static_cast<int>(
                std::sqrt(static_cast<float>(radius * radius - y * y)));
            SDL_RenderDrawLine(renderer, cx - halfWidth, cy + y,
                               cx + halfWidth, cy + y);
        }
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const AreaEffect& effect : m_areaEffects) {
        const float progress = effect.duration > 0.0f
            ? 1.0f - effect.ttl / effect.duration
            : 1.0f;
        const int radius = static_cast<int>(effect.radius * (0.35f + progress * 0.65f));
        SDL_Color color = effect.color;
        color.a = static_cast<Uint8>(130.0f * (1.0f - progress));
        drawFilledCircle(static_cast<int>(effect.x - cameraX),
                         static_cast<int>(effect.y - cameraY),
                         radius, color);
    }

    for (const Projectile& shot : m_projectiles) {
        drawFilledCircle(static_cast<int>(shot.x - cameraX),
                         static_cast<int>(shot.y - cameraY),
                         static_cast<int>(shot.radius),
                         shot.color);
    }

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

int EnemySystem::damageInRect(const SDL_Rect& hitbox, int damage) {
    for (Enemy& enemy : m_enemies) {
        if (enemy.invuln <= 0.0f && enemyIntersectsRect(enemy, hitbox)) {
            enemy.health -= damage;
            enemy.invuln = kHitCooldown;
        }
    }
    const int killed = static_cast<int>(
        std::count_if(m_enemies.begin(), m_enemies.end(),
                      [](const Enemy& enemy) { return enemy.health <= 0; }));
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
                       [](const Enemy& enemy) { return enemy.health <= 0; }),
        m_enemies.end());
    return killed;
}

int EnemySystem::damageCircle(float x, float y, float radius, int damage,
                              float knockbackDirectionX, float knockbackDirectionY,
                              float knockbackPower, bool* hit) {
    bool anyHit = false;
    for (Enemy& enemy : m_enemies) {
        if (enemy.invuln <= 0.0f && enemyIntersectsCircle(enemy, x, y, radius)) {
            enemy.health -= damage;
            enemy.invuln = kHitCooldown;
            // Heavier/sturdier types are shoved less.
            const float kb = knockbackPower * (1.0f - typeOf(enemy).knockbackResist);
            enemy.x += knockbackDirectionX * kb;
            enemy.y += knockbackDirectionY * kb;
            anyHit = true;
        }
    }
    if (hit) {
        *hit = anyHit;
    }
    const int killed = static_cast<int>(
        std::count_if(m_enemies.begin(), m_enemies.end(),
                      [](const Enemy& enemy) { return enemy.health <= 0; }));
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
                       [](const Enemy& enemy) { return enemy.health <= 0; }),
        m_enemies.end());
    return killed;
}

int EnemySystem::damagePlayer(float playerX, float playerY, float playerSize) {
    const float cx = playerX + playerSize * 0.5f;
    const float cy = playerY + playerSize * 0.5f;
    const float playerRadius = playerSize * 0.45f;
    int damage = 0;

    for (const Enemy& enemy : m_enemies) {
        const float dx = enemy.x - cx;
        const float dy = enemy.y - cy;
        const float combined = typeOf(enemy).radius + playerRadius;
        if (dx * dx + dy * dy <= combined * combined) {
            damage = std::max(damage, 5);
        }
    }

    for (Projectile& shot : m_projectiles) {
        const float dx = shot.x - cx;
        const float dy = shot.y - cy;
        const float combined = shot.radius + playerRadius;
        if (dx * dx + dy * dy <= combined * combined) {
            damage = std::max(damage, shot.damage * 10);
            shot.ttl = 0.0f;
        }
    }

    for (const AreaEffect& effect : m_areaEffects) {
        if (effect.damage <= 0) {
            continue;
        }
        const float dx = effect.x - cx;
        const float dy = effect.y - cy;
        const float combined = effect.radius + playerRadius;
        if (dx * dx + dy * dy <= combined * combined) {
            damage = std::max(damage, effect.damage * 10);
        }
    }

    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                       [](const Projectile& shot) { return shot.ttl <= 0.0f; }),
        m_projectiles.end());
    return damage;
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
