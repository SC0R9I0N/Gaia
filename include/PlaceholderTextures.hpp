#pragma once

#include <SDL.h>

namespace gaia {

// The kinds of game objects that need a texture. Each kind maps to one PNG in
// assets/ (see assetFileName() in the .cpp) AND to a procedural fallback, so the
// game always renders something even with no art files present.
//
// ART TEAM: to replace any of these, just drop a PNG with the matching name into
// assets/ (player.png, enemy.png, vendor.png, floor.png, attack.png, ...). No
// code change or rebuild needed — the file is loaded at runtime and used in
// place of the procedural stand-in. assetFileName() in PlaceholderTextures.cpp
// is the single source of truth for which file maps to which kind.
enum class AssetKind {
    Character,   // the player (and future NPCs)
    Character2,
    Character3,
    Character4,
    Character5,
    Character6,
    Dash,        // player dash / roll animation sprite sheet
    Dash2,
    Dash3,
    Dash4,
    Dash5,
    Dash6,
    Item,        // pickups and usables
    Background,  // the full-window backdrop
    Vendor,      // hub vendors / upgrade stations
    RuneVendor,  // hub rune configuration engine
    ShadyVendor, // hub risky item dealer
    ConsciousnessConsole, // hub character selection console
    SkillTreeConsole, // hub skill tree selector
    ArtifactStorage, // hub run artifact selector
    RunPortal,   // animated portal for starting a run
    GoldCurrency,
    SkillCurrency,
    ShadyCurrency,
    Enemy,       // hostile creatures
    Floor,       // tiled room floor
    Attack,      // melee swing / attack animation
    HubFloor,    // hub: seamless sandstone brick floor tile
    HubWall,     // hub: seamless sandstone brick wall tile
    HubLantern,  // hub: animated wall-lantern sprite sheet
    HubPillar,   // hub: sandstone column decoration
    HubUrn,      // hub: sandstone urn decoration
    HubBanner,   // hub: hanging banner decoration
};

// Lazily creates and caches simple, procedurally-drawn SDL textures so every
// game object can render a recognizable placeholder without any art files.
//
// Each AssetKind maps to one default texture, generated the first time it is
// requested. Drop real .png files into assets/ and load them here later to
// replace the stand-ins without touching the rest of the game.
class PlaceholderTextures {
public:
    PlaceholderTextures() = default;
    ~PlaceholderTextures();

    // Non-copyable: it owns raw SDL textures.
    PlaceholderTextures(const PlaceholderTextures&) = delete;
    PlaceholderTextures& operator=(const PlaceholderTextures&) = delete;

    // Records the renderer and the size to use for the full-screen background.
    void init(SDL_Renderer* renderer, int backgroundWidth, int backgroundHeight);

    // Returns the default placeholder texture for the given kind, creating it on
    // first use. Returns nullptr only if init() was never called.
    SDL_Texture* defaultFor(AssetKind kind);

    // Frees every cached texture. Safe to call more than once.
    void destroy();

private:
    // The PNG (under assets/) an artist can drop in to override a kind, or
    // nullptr for kinds that have no file mapping. Single source of truth.
    static const char* assetFileName(AssetKind kind);

    // Loads a PNG into a texture, or returns nullptr if the file is missing.
    SDL_Texture* loadFromFile(const char* path);

    // Procedural fallbacks, used when the matching PNG is absent.
    SDL_Texture* makeCharacter();
    SDL_Texture* makeDash();
    SDL_Texture* makeItem();
    SDL_Texture* makeBackground();
    SDL_Texture* makeVendor();
    SDL_Texture* makeRuneVendor();
    SDL_Texture* makeShadyVendor();
    SDL_Texture* makeConsciousnessConsole();
    SDL_Texture* makeSkillTreeConsole();
    SDL_Texture* makeArtifactStorage();
    SDL_Texture* makeRunPortal();
    SDL_Texture* makeGoldCurrency();
    SDL_Texture* makeSkillCurrency();
    SDL_Texture* makeShadyCurrency();
    SDL_Texture* makeEnemy();
    SDL_Texture* makeFloor();
    SDL_Texture* makeAttack();
    SDL_Texture* makeProcedural(AssetKind kind);

    SDL_Renderer* m_renderer = nullptr;
    int m_bgWidth  = 0;
    int m_bgHeight = 0;

    SDL_Texture* m_character  = nullptr;
    SDL_Texture* m_character2 = nullptr;
    SDL_Texture* m_character3 = nullptr;
    SDL_Texture* m_character4 = nullptr;
    SDL_Texture* m_character5 = nullptr;
    SDL_Texture* m_character6 = nullptr;
    SDL_Texture* m_dash       = nullptr;
    SDL_Texture* m_dash2      = nullptr;
    SDL_Texture* m_dash3      = nullptr;
    SDL_Texture* m_dash4      = nullptr;
    SDL_Texture* m_dash5      = nullptr;
    SDL_Texture* m_dash6      = nullptr;
    SDL_Texture* m_item       = nullptr;
    SDL_Texture* m_background  = nullptr;
    SDL_Texture* m_vendor      = nullptr;
    SDL_Texture* m_runeVendor  = nullptr;
    SDL_Texture* m_shadyVendor = nullptr;
    SDL_Texture* m_consciousnessConsole = nullptr;
    SDL_Texture* m_skillTreeConsole = nullptr;
    SDL_Texture* m_artifactStorage = nullptr;
    SDL_Texture* m_runPortal = nullptr;
    SDL_Texture* m_goldCurrency = nullptr;
    SDL_Texture* m_skillCurrency = nullptr;
    SDL_Texture* m_shadyCurrency = nullptr;
    SDL_Texture* m_enemy       = nullptr;
    SDL_Texture* m_floor       = nullptr;
    SDL_Texture* m_attack      = nullptr;
    SDL_Texture* m_hubFloor    = nullptr;
    SDL_Texture* m_hubWall     = nullptr;
    SDL_Texture* m_hubLantern  = nullptr;
    SDL_Texture* m_hubPillar   = nullptr;
    SDL_Texture* m_hubUrn      = nullptr;
    SDL_Texture* m_hubBanner   = nullptr;
};

}  // namespace gaia
