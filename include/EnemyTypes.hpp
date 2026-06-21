#pragma once

#include <SDL.h>

#include <vector>

namespace gaia {

// Broad behavioural category for an enemy. Movement AI is currently the same
// "walk toward the player" for everyone; these classes exist so future combat
// code can branch behaviour per archetype without reshuffling the data model.
enum class EnemyClass {
    Melee,    // walks up and strikes
    Swarm,    // small, fast, fragile; meant to attack in numbers
    Charger,  // closes distance in fast bursts
    Ranged,   // keeps distance and shoots
    Bomber,   // rushes in and detonates
    Tank,     // big, slow, heavy hitter
    Caster,   // summons minions / casts area effects
};

// The kind of offensive ability a type uses. NOTE: nothing fires these yet —
// they are data describing each archetype's intended attack so the actual
// behaviour can be wired up later without touching every spawn site.
enum class EnemyAbilityKind {
    None,
    MeleeStrike,
    RangedShot,
    ChargeDash,
    Explode,
    GroundSmash,
    SummonMinions,
    ArcaneBlast,
};

// One ability's tuning. Purely descriptive for now.
struct EnemyAbility {
    EnemyAbilityKind kind = EnemyAbilityKind::None;
    float cooldown = 0.0f;  // seconds between uses
    float range    = 0.0f;  // activation / effective range, world px
    int   damage   = 0;     // damage dealt on a hit
};

// A full enemy archetype: its stats, appearance, and (future) ability. Every
// live enemy stores an index into the registry below and reads its stats from
// here, so tuning a type — or adding a new one — happens in one place.
struct EnemyType {
    const char*  name;
    EnemyClass   cls;
    float        radius;          // body radius in px (its on-screen size)
    float        speed;           // movement speed, px/s
    int          health;          // hits to kill
    SDL_Color    color;           // body colour (tints the shared sprite)
    float        knockbackResist; // 0 = full knockback, 1 = immovable
    EnemyAbility ability;         // primary ability (not yet fired)
};

// Every archetype the game can spawn. To add an enemy, append one entry here —
// spawners pick from this list at random, so it is automatically included.
const std::vector<EnemyType>& enemyTypeRegistry();

}  // namespace gaia
