#pragma once

#include <SDL.h>

namespace gaia {

// The kinds of game objects that need a visible stand-in until real art exists.
enum class AssetKind {
    Character,   // the player and (future) NPCs / enemies
    Item,        // pickups and usables
    Background,  // the level backdrop
    Vendor,      // hub vendors / upgrade stations
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
    SDL_Texture* makeCharacter();
    SDL_Texture* makeItem();
    SDL_Texture* makeBackground();
    SDL_Texture* makeVendor();

    SDL_Renderer* m_renderer = nullptr;
    int m_bgWidth  = 0;
    int m_bgHeight = 0;

    SDL_Texture* m_character  = nullptr;
    SDL_Texture* m_item       = nullptr;
    SDL_Texture* m_background  = nullptr;
    SDL_Texture* m_vendor      = nullptr;
};

}  // namespace gaia
