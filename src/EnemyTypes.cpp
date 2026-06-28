#include "EnemyTypes.hpp"

namespace gaia {

// The enemy bestiary. Each row varies size (radius), speed, health, durability
// (knockbackResist), movement class, and primary ability. Keep this readable: it
// is the single source of truth designers tweak.
//
//   name        class             radius speed  hp  colour (r,g,b,a)        kbRes  ability {kind, cooldown, range, damage}
const std::vector<EnemyType>& enemyTypeRegistry() {
    static const std::vector<EnemyType> types = {
        {"Grunt",     EnemyClass::Melee,    22.0f,  95.0f, 3, SDL_Color{210,  70,  60, 255}, 0.0f,
            {EnemyAbilityKind::MeleeStrike,   1.2f,  60.0f, 1}},
        {"Swarmling", EnemyClass::Swarm,    14.0f, 152.0f, 1, SDL_Color{150, 200,  90, 255}, 0.0f,
            {EnemyAbilityKind::MeleeStrike,   0.8f,  40.0f, 1}},
        {"Brute",     EnemyClass::Tank,     34.0f,  52.0f, 8, SDL_Color{120, 128, 156, 255}, 0.7f,
            {EnemyAbilityKind::GroundSmash,   2.6f,  80.0f, 2}},
        {"Spitter",   EnemyClass::Ranged,   20.0f,  72.0f, 3, SDL_Color{172,  92, 200, 255}, 0.1f,
            {EnemyAbilityKind::RangedShot,    1.8f, 480.0f, 1}},
        {"Charger",   EnemyClass::Charger,  24.0f, 120.0f, 4, SDL_Color{236, 140,  52, 255}, 0.2f,
            {EnemyAbilityKind::ChargeDash,    3.0f, 420.0f, 2}},
        {"Bomber",    EnemyClass::Bomber,   18.0f, 112.0f, 2, SDL_Color{240, 212,  72, 255}, 0.0f,
            {EnemyAbilityKind::Explode,       0.0f,  50.0f, 3}},
        {"Sentinel",  EnemyClass::Ranged,   28.0f,  30.0f, 6, SDL_Color{ 92, 182, 202, 255}, 0.5f,
            {EnemyAbilityKind::RangedShot,    2.2f, 600.0f, 2}},
        {"Warden",    EnemyClass::Caster,   30.0f,  60.0f, 7, SDL_Color{126, 112, 230, 255}, 0.4f,
            {EnemyAbilityKind::SummonMinions, 5.0f, 320.0f, 0}},
    };
    return types;
}

}  // namespace gaia
