#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <SDL_ttf.h>

#include "Keybindings.hpp"
#include "Room.hpp"
#include "Enemy.hpp"
#include "PlaceholderTextures.hpp"

// Forward declarations keep SDL out of the public header.
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;
struct SDL_Color;
struct SDL_Rect;

namespace gaia {

class Player;
class PlaceholderTextures;

// Owns the window, the render loop, and the lifetime of the game.
//
// Typical usage:
//     gaia::Game game;
//     if (game.init("Gaia", 1280, 720)) {
//         game.run();
//     }
// (run() calls shutdown() automatically when the loop ends.)
class Game {
public:
    Game();
    ~Game();

    // Non-copyable: it owns raw SDL resources.
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Creates the window and renderer. Returns false on failure.
    bool init(const char* title, int width, int height);

    // Runs the main loop until the window is closed. Blocks until then.
    void run();

private:
    // Which screen the pause overlay is showing.
    enum class PauseScreen { Main, Settings, ConfirmQuit };
    // How the window is presented.
    enum class DisplayMode { Windowed, Fullscreen, BorderlessWindowed, BorderlessFullscreen };
    // Top-level pages inside Settings.
    enum class SettingsTab { Video, Audio, Controls, Gameplay };
    // Which settings dropdown, if any, is currently expanded.
    enum class Dropdown { None, Resolution, DisplayMode };
    // Equipment slots intentionally stay separate from the bag so future stat
    // systems can read gear directly.
    enum class EquipmentSlot { Weapon, Armor, Trinket, Relic, Artifact, None };
    enum class InventoryItemKind { Generic, Rune };

    struct InventoryItem {
        const char* name = "";
        const char* description = "";
        EquipmentSlot equipSlot = EquipmentSlot::None;
        SDL_Color color{255, 255, 255, 255};
        InventoryItemKind kind = InventoryItemKind::Generic;
        std::array<int, 3> containedSpells{-1, -1, -1};
        int containedSpellCount = 0;
        bool extracted = false;
    };

    struct DraggedInventoryItem {
        bool active = false;
        bool fromEquipment = false;
        int fromSlot = -1;
        int itemIndex = -1;
    };

    struct RunSnapshot {
        bool valid = false;
        int skillCurrency = 0;
        int shadyCurrency = 0;
        int selectedCharacterIndex = 0;
        int selectedArtifactIndex = 0;
        bool shadyPurchaseMade = false;
        std::vector<bool> researchedSpells;
        std::vector<bool> artifactUnlocked;
        std::vector<bool> runeExtracted;
        std::vector<int> loadout;
        std::vector<int> inventorySlots;
        std::vector<int> equipmentSlots;
    };

    struct DraggedRune {
        bool active = false;
        bool fromLoadout = false;
        int fromSlot = -1;
        int spellIndex = -1;
    };

    struct DraggedResearchRune {
        bool active = false;
        int itemIndex = -1;
    };

    struct CharacterOption {
        const char* name = "";
        const char* subtitle = "";
        AssetKind texture = AssetKind::Character;
        bool unlocked = true;
    };

    struct ArtifactOption {
        const char* name = "";
        const char* description = "";
        const char* challenge = "";
        SDL_Color color{255, 255, 255, 255};
        bool unlocked = false;
        int itemIndex = -1;
    };

    void processEvents();
    void startGameFromMenu();
    void acceptRunPrompt();
    void exitRunToHub();
    void handlePlayerDeath();
    void handleMainMenuEvent(const SDL_Event& event);
    void handleMainMenuClick(int mouseX, int mouseY);
    void handleGameEvent(const SDL_Event& event);
    void handlePauseEvent(const SDL_Event& event);
    void handlePauseClick(int mouseX, int mouseY);
    void handleSpellbookEvent(const SDL_Event& event);
    void handleInventoryEvent(const SDL_Event& event);
    void handleRuneConfigEvent(const SDL_Event& event);
    void handleShadyShopEvent(const SDL_Event& event);
    void handleConsciousnessEvent(const SDL_Event& event);
    void handleSkillTreeEvent(const SDL_Event& event);
    void handleArtifactStorageEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void updateCamera();
    void render();
    void shutdown();
    void drawFpsOverlay();
    void drawCurrencyHud();
    // Bottom-center spell hotbar: equipped spells as icon boxes that light up to
    // show which spells the in-progress cast chain can still reach.
    void drawSpellHotbar();
    void renderRunPrompt();
    // The "Start a run?" prompt panel and its clickable Yes/No buttons. Shared
    // by the renderer and the click hit-test so they never drift.
    SDL_Rect runPromptPanelRect() const;
    SDL_Rect runPromptYesButtonRect() const;
    SDL_Rect runPromptNoButtonRect() const;
    void renderMainMenu();
    void renderPauseMenu();
    void renderSettingsMenu();
    void renderConfirmQuitMenu();
    void renderSpellbook();
    void renderInventory();
    void renderRuneConfig();
    void renderShadyShop();
    void renderConsciousnessConsole();
    void renderSkillTreeSelector();
    void renderArtifactStorage();

    // Draws the game's own cursor (a crosshair) at the current mouse position.
    void drawCursor();
    // Centers `text` on (cx, cy) and draws it; returns the rect it occupied.
    SDL_Rect drawTextCentered(const char* text, int cx, int cy, SDL_Color color);
    SDL_Rect drawTextWrappedCentered(const char* text, int cx, int cy,
                                     int wrapWidth, SDL_Color color);
    // Layout for clickable menu rows: row `index` of `count` evenly stacked rows.
    SDL_Rect menuRowRect(int index, int count) const;

    // ---- Resolution / scaling ---------------------------------------------
    // Reconfigures the window for the current resolution + display mode. The
    // logical render size stays fixed, so the pixel art simply scales up.
    void applyVideoMode(bool persist = true);
    void applyResolution(int index);   // pick resolution, then applyVideoMode
    void setDisplayMode(int mode);      // pick display mode, then applyVideoMode
    void applyVSync();
    bool recreateRenderer();
    void loadVideoSettings();
    void saveVideoSettings() const;
    // The whole game is drawn into a fixed logical-size canvas texture, which is
    // then blitted (letterboxed) to the window. This keeps gameplay/UI at a
    // constant coordinate space no matter the window resolution, and — crucially
    // — never touches SDL's renderer scaling, so mouse coordinates are always
    // real window pixels that we map ourselves.
    void beginFrame();    // start drawing into the canvas
    void presentFrame();  // blit the canvas to the window (letterboxed) + present
    // The destination rectangle (in window output pixels) the canvas is scaled
    // into: the logical space fit to the window, preserving aspect, centered.
    SDL_Rect canvasRect() const;
    // Current drawable/output size of the window, independent of the active
    // render target. SDL_GetRendererOutputSize can report the canvas texture
    // size while drawing into it, which breaks mouse conversion after resizing.
    SDL_Point windowOutputSize() const;
    // Converts a real window-space position to logical canvas coordinates by
    // inverting the exact blit rectangle canvasRect() produces.
    SDL_Point windowToLogical(int windowX, int windowY) const;
    // The current mouse position in logical space (for the cursor / hover tests).
    SDL_Point mouseLogical() const;
    // Number of clickable rows on the settings screen, and the rects of the
    // tab buttons, clickable rows, and dropdown options.
    int  settingsRowCount() const;
    SDL_Rect settingsTabRect(int index) const;
    SDL_Rect settingsRowRect(int index) const;
    SDL_Rect resolutionOptionRect(int slot) const;
    SDL_Rect displayModeOptionRect(int index) const;
    void initializeInventory();
    SDL_Rect inventoryBagSlotRect(int index) const;
    SDL_Rect inventoryEquipmentSlotRect(int index) const;
    int inventorySlotAt(int mouseX, int mouseY, bool* equipment) const;
    bool inventorySlotAccepts(bool equipment, int slot, int itemIndex) const;
    bool moveInventoryItem(bool fromEquipment, int fromSlot,
                           bool toEquipment, int toSlot);
    bool addInventoryItem(const InventoryItem& item);
    SDL_Rect runeConfigActiveSlotRect(int index) const;
    SDL_Rect runeConfigLibrarySlotRect(int index) const;
    SDL_Rect runeResearchButtonRect() const;
    SDL_Rect runeResearchInventoryRuneRect(int index) const;
    SDL_Rect runeResearchExtractionSlotRect() const;
    int runeConfigActiveSlotAt(int mouseX, int mouseY) const;
    int runeConfigLibrarySlotAt(int mouseX, int mouseY) const;
    int runeResearchInventoryRuneAt(int mouseX, int mouseY) const;
    SDL_Rect shadyShopItemRect(int index) const;
    SDL_Rect consciousnessCardRect(int offset) const;
    SDL_Rect consciousnessLeftButtonRect() const;
    SDL_Rect consciousnessRightButtonRect() const;
    SDL_Rect artifactCardRect(int index) const;
    void rotateCharacterSelection(int direction);
    const CharacterOption& selectedCharacter() const;
    void awardEnemyCurrencies(int killedCount);
    bool selectArtifact(int index);
    bool extractRuneItem(int itemIndex);
    bool isSpellResearched(int spellIndex) const;
    void loadProgress();
    void saveProgress() const;
    void ensureStarterDefaults();
    void captureRunSnapshot();
    void restoreRunSnapshot();
    void removeNonRuneInventory();
    void setPlayerHealth(int value);
    static const char* equipmentSlotName(EquipmentSlot slot);

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture*  m_canvas   = nullptr;  // fixed logical-size render target
    bool          m_running  = false;
    bool          m_mainMenuOpen = true;
    bool          m_paused   = false;
    bool          m_spellbookOpen = false;
    bool          m_inventoryOpen = false;
    bool          m_runeConfigOpen = false;
    bool          m_shadyShopOpen = false;
    bool          m_consciousnessOpen = false;
    bool          m_skillTreeOpen = false;
    bool          m_artifactStorageOpen = false;
    bool          m_runPromptOpen = false;
    bool          m_runPromptDismissed = false;
    bool          m_mainMenuQuitConfirm = false;

    PauseScreen m_pauseScreen = PauseScreen::Main;
    // Action currently waiting for a new key, or -1 when not rebinding.
    int m_rebindingAction = -1;
    Keybindings m_keybinds;

    // Current window resolution (index into the resolution table), the scroll
    // offset of the resolution dropdown, the current display mode, and which
    // settings dropdown is expanded.
    int         m_resIndex = 0;
    int         m_resScroll = 0;
    DisplayMode m_displayMode = DisplayMode::Windowed;
    SettingsTab m_settingsTab = SettingsTab::Video;
    Dropdown    m_openDropdown = Dropdown::None;
    bool        m_vsync = true;
    bool        m_integerScaling = false;
    bool        m_showFps = false;
    int         m_frameLimitIndex = 2;
    int         m_masterVolume = 100;
    int         m_musicVolume = 80;
    int         m_sfxVolume = 80;
    bool        m_muteWhenUnfocused = false;
    bool        m_pauseOnFocusLoss = true;
    bool        m_customCursor = true;
    bool        m_tutorialHints = true;
    float       m_fpsTimer = 0.0f;
    int         m_fpsFrames = 0;
    int         m_currentFps = 0;

    static constexpr int kInventoryColumns = 5;
    static constexpr int kInventoryRows = 4;
    static constexpr int kInventorySlotCount = kInventoryColumns * kInventoryRows;
    static constexpr int kEquipmentSlotCount = 5;
    std::vector<InventoryItem> m_inventoryItems;
    std::vector<InventoryItem> m_shadyShopItems;
    std::array<int, kInventorySlotCount> m_inventorySlots{};
    std::array<int, kEquipmentSlotCount> m_equipmentSlots{};
    DraggedInventoryItem m_draggedInventoryItem;
    int m_selectedRuneLoadoutSlot = 0;
    DraggedRune m_draggedRune;
    DraggedResearchRune m_draggedResearchRune;
    bool m_runeResearchOpen = false;
    int m_runeLibraryScrollOffset = 0;
    std::vector<bool> m_researchedSpells;
    bool m_shadyPurchaseMade = false;
    int m_selectedCharacterIndex = 0;
    std::array<CharacterOption, 6> m_characterOptions{};
    std::vector<ArtifactOption> m_artifactOptions;
    int m_selectedArtifactIndex = 0;
    int m_artifactScrollOffset = 0;
    int m_gold = 0;
    int m_skillCurrency = 0;
    int m_shadyCurrency = 0;
    int m_playerHealth = 100;
    int m_playerMaxHealth = 100;
    float m_playerDisplayedHealth = 100.0f;
    float m_playerDamageCooldown = 0.0f;
    bool m_roomClearRewarded = false;
    RunSnapshot m_runSnapshot;
    bool m_progressReady = false;
    std::mt19937 m_currencyRng{std::random_device{}()};

    // Fixed logical render size. Gameplay, rooms, and UI all live in this space;
    // SDL scales it to the actual window resolution (so art scales with it).
    int m_width  = 0;
    int m_height = 0;
    float m_cameraX = 0.0f;
    float m_cameraY = 0.0f;

    TTF_Font* m_font = nullptr;

    RoomSystem m_rooms;
    EnemySystem m_enemies;
    float m_playerSafeX = 0.0f;
    float m_playerSafeY = 0.0f;

    // Held by pointer so SDL types stay out of this header (see forward decls).
    std::unique_ptr<PlaceholderTextures> m_textures;
    std::unique_ptr<Player>              m_player;
};

}  // namespace gaia
