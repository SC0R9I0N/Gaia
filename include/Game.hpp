#pragma once

#include <cstdint>
#include <memory>
#include <SDL_ttf.h>

#include "Keybindings.hpp"
#include "Room.hpp"
#include "Enemy.hpp"

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

    void processEvents();
    void startGameFromMenu();
    void acceptRunPrompt();
    void handleMainMenuEvent(const SDL_Event& event);
    void handleMainMenuClick(int mouseX, int mouseY);
    void handleGameEvent(const SDL_Event& event);
    void handlePauseEvent(const SDL_Event& event);
    void handlePauseClick(int mouseX, int mouseY);
    void handleSpellbookEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void updateCamera();
    void render();
    void shutdown();
    void drawFpsOverlay();
    void renderRunPrompt();
    void renderMainMenu();
    void renderPauseMenu();
    void renderSettingsMenu();
    void renderConfirmQuitMenu();
    void renderSpellbook();

    // Draws the game's own cursor (a crosshair) at the current mouse position.
    void drawCursor();
    // Centers `text` on (cx, cy) and draws it; returns the rect it occupied.
    SDL_Rect drawTextCentered(const char* text, int cx, int cy, SDL_Color color);
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

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture*  m_canvas   = nullptr;  // fixed logical-size render target
    bool          m_running  = false;
    bool          m_mainMenuOpen = true;
    bool          m_paused   = false;
    bool          m_spellbookOpen = false;
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

    // Fixed logical render size. Gameplay, rooms, and UI all live in this space;
    // SDL scales it to the actual window resolution (so art scales with it).
    int m_width  = 0;
    int m_height = 0;
    float m_cameraX = 0.0f;
    float m_cameraY = 0.0f;

    TTF_Font* m_font = nullptr;

    RoomSystem m_rooms;
    EnemySystem m_enemies;

    // Held by pointer so SDL types stay out of this header (see forward decls).
    std::unique_ptr<PlaceholderTextures> m_textures;
    std::unique_ptr<Player>              m_player;
};

}  // namespace gaia
