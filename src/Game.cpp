#include "Game.hpp"

#include "PlaceholderTextures.hpp"
#include "Player.hpp"
#include "SpellCaster.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace gaia {

namespace {
// Selectable window resolutions. The game renders at a fixed logical size and
// SDL scales the result to whichever of these the player picks, so a higher
// resolution simply makes the pixel art larger and crisper. Covers 4:3, 16:10,
// 16:9, and ultrawide (21:9 / 32:9) aspect ratios.
struct Resolution {
    int w;
    int h;
    const char* label;
};
constexpr Resolution kResolutions[] = {
    {1024,  768, "1024 x 768  (4:3)"},
    {1280,  720, "1280 x 720  (16:9)"},
    {1280,  800, "1280 x 800  (16:10)"},
    {1366,  768, "1366 x 768  (16:9)"},
    {1440,  900, "1440 x 900  (16:10)"},
    {1600,  900, "1600 x 900  (16:9)"},
    {1680, 1050, "1680 x 1050  (16:10)"},
    {1920, 1080, "1920 x 1080  (16:9)"},
    {1920, 1200, "1920 x 1200  (16:10)"},
    {2048, 1152, "2048 x 1152  (16:9)"},
    {2560, 1080, "2560 x 1080  (21:9)"},
    {2560, 1440, "2560 x 1440  (16:9)"},
    {2560, 1600, "2560 x 1600  (16:10)"},
    {3440, 1440, "3440 x 1440  (21:9)"},
    {3840, 1080, "3840 x 1080  (32:9)"},
    {3840, 1600, "3840 x 1600  (21:9)"},
    {3840, 2160, "3840 x 2160  (16:9)"},
    {5120, 1440, "5120 x 1440  (32:9)"},
    {5120, 2160, "5120 x 2160  (21:9)"},
};
constexpr int kResolutionCount =
    static_cast<int>(sizeof(kResolutions) / sizeof(kResolutions[0]));

// How many resolution rows are visible at once in the (scrollable) dropdown.
constexpr int kResVisible = 8;

// Display modes, in selection order.
constexpr const char* kDisplayModeLabels[] = {
    "Windowed", "Fullscreen", "Borderless Windowed", "Borderless Fullscreen"};
constexpr int kDisplayModeCount =
    static_cast<int>(sizeof(kDisplayModeLabels) / sizeof(kDisplayModeLabels[0]));

constexpr const char* kSettingsTabLabels[] = {
    "Video", "Audio", "Controls", "Gameplay"};
constexpr int kSettingsTabCount =
    static_cast<int>(sizeof(kSettingsTabLabels) / sizeof(kSettingsTabLabels[0]));

constexpr const char* kFrameLimitLabels[] = {"Off", "30 FPS", "60 FPS", "120 FPS"};
constexpr int kFrameLimitValues[] = {0, 30, 60, 120};
constexpr int kFrameLimitCount =
    static_cast<int>(sizeof(kFrameLimitLabels) / sizeof(kFrameLimitLabels[0]));

// Extra world-space margin visible around room boundaries. This lets wall/border
// art sit outside the playable collision rectangle without being clipped by the
// camera clamp.
constexpr int kCameraBoundaryBuffer = 128;

const char* onOff(bool enabled) {
    return enabled ? "On" : "Off";
}

int clampPercent(int value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

void stepPercent(int& value) {
    value += 10;
    if (value > 100) value = 0;
}
}  // namespace

static SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path) {
    int width, height, channels;
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, 4);
    if (!pixels) return nullptr;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    stbi_image_free(pixels);

    return texture;
}

Game::Game() = default;

Game::~Game() {
    // Safe to call even if init() failed or run() already cleaned up.
    shutdown();
}

bool Game::init(const char* title, int width, int height) {
    // The passed size is the fixed logical render space; the window resolution
    // is chosen separately (and may scale the art up).
    m_width  = width;
    m_height = height;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    // Nearest-neighbor scaling keeps pixel art crisp when scaled up.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    // Pick the saved resolution (defaults to the logical size on first run).
    loadVideoSettings();
    const Resolution& res = kResolutions[m_resIndex];

    m_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        res.w,
        res.h,
        SDL_WINDOW_SHOWN);
    if (!m_window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // -1 picks the first accelerated driver that supports the requested flags.
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, m_vsync ? "1" : "0");
    m_renderer = SDL_CreateRenderer(
        m_window,
        -1,
        SDL_RENDERER_ACCELERATED |
            (m_vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!m_renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    // Everything is rendered into this fixed logical-size canvas, which is then
    // scaled (letterboxed) onto the window in presentFrame(). We deliberately do
    // NOT use SDL_RenderSetLogicalSize / SDL_RenderSetScale on the window: those
    // make SDL silently rewrite mouse-event coordinates and keep mutable scaling
    // state that can go stale after a resolution change. Rendering into a
    // constant-size texture instead keeps the game's coordinate space fixed and
    // leaves every mouse coordinate as a real, unscaled window pixel.
    m_canvas = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        m_width,
        m_height);
    if (!m_canvas) {
        std::fprintf(stderr, "SDL_CreateTexture (canvas) failed: %s\n", SDL_GetError());
        return false;
    }

    // Apply the saved display mode (fullscreen/borderless) without re-saving.
    applyVideoMode(false);
    applyVSync();

    // Build the placeholder texture set and the rooms, then drop the player in
    // the center of the spawn room.
    m_textures = std::make_unique<PlaceholderTextures>();
    m_textures->init(m_renderer, m_width, m_height);

    m_rooms.init(m_width, m_height);

    m_player = std::make_unique<Player>();
    const float half = m_player->size() * 0.5f;
    const SDL_Point spawn = m_rooms.spawnCenter();
    m_player->init(m_textures.get(),
                   static_cast<float>(spawn.x) - half,
                   static_cast<float>(spawn.y) - half);
    updateCamera();

    // Hide the OS cursor when the custom pixel cursor is enabled.
    SDL_ShowCursor(m_customCursor ? SDL_DISABLE : SDL_ENABLE);

    // Apply any keybinds the player saved in a previous session.
    m_keybinds.load();

    m_running = true;
    m_mainMenuOpen = true;
    m_paused = false;

    if (TTF_Init() == -1) {
    // TTF_GetError() for details
    }
    
    m_font = TTF_OpenFont("assets/fonts/font.ttf", 24);
    if (!m_font) {
        std::fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
        return false;
    }

    return true;
}

void Game::run() {
    // Fixed reference for delta-time so movement is frame-rate independent.
    Uint64 previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (m_running) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const float delta = static_cast<float>((now - previous) / frequency);
        previous = now;

        processEvents();
        if (m_mainMenuOpen) {
            renderMainMenu();
        } else if (m_paused) {
            renderPauseMenu();
        } else if (m_spellbookOpen) {
            renderSpellbook();
        } else {
            update(delta);
            render();
        }

        const int frameLimit = kFrameLimitValues[m_frameLimitIndex];
        if (frameLimit > 0) {
            const double elapsedMs =
                (SDL_GetPerformanceCounter() - now) * 1000.0 / frequency;
            const double targetMs = 1000.0 / frameLimit;
            if (elapsedMs < targetMs) {
                SDL_Delay(static_cast<Uint32>(targetMs - elapsedMs));
            }
        }

        const Uint64 frameEnd = SDL_GetPerformanceCounter();
        const float frameSeconds =
            static_cast<float>((frameEnd - now) / frequency);
        ++m_fpsFrames;
        m_fpsTimer += frameSeconds;
        if (m_fpsTimer >= 1.0f) {
            m_currentFps = static_cast<int>(m_fpsFrames / m_fpsTimer + 0.5f);
            m_fpsFrames = 0;
            m_fpsTimer = 0.0f;
        }
    }
    shutdown();
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
            continue;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_FOCUS_LOST &&
            m_pauseOnFocusLoss && !m_mainMenuOpen && !m_paused) {
            m_paused = true;
            m_pauseScreen = PauseScreen::Main;
            m_rebindingAction = -1;
            m_openDropdown = Dropdown::None;
            continue;
        }
        // F11 toggles fullscreen from anywhere. Also a reliable way back to a
        // Windowed view if a fullscreen mode ever misbehaves.
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
            setDisplayMode(static_cast<int>(
                m_displayMode == DisplayMode::Windowed
                    ? DisplayMode::BorderlessFullscreen
                    : DisplayMode::Windowed));
            continue;
        }
        if (m_mainMenuOpen) {
            handleMainMenuEvent(event);
        } else if (m_paused) {
            handlePauseEvent(event);
        } else if (m_spellbookOpen) {
            handleSpellbookEvent(event);
        } else {
            handleGameEvent(event);
        }
    }
}

void Game::handleMainMenuEvent(const SDL_Event& event) {
    if (m_pauseScreen == PauseScreen::Settings) {
        handlePauseEvent(event);
        return;
    }

    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_RETURN ||
                event.key.keysym.sym == SDLK_KP_ENTER ||
                event.key.keysym.sym == SDLK_SPACE) {
                startGameFromMenu();
            } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                m_mainMenuQuitConfirm = !m_mainMenuQuitConfirm;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                handleMainMenuClick(lp.x, lp.y);
            }
            break;
        default:
            break;
    }
}

void Game::handleMainMenuClick(int mouseX, int mouseY) {
    if (m_mainMenuQuitConfirm) {
        constexpr int kCount = 2;  // Quit to Desktop, Cancel
        auto hit = [&](int index) {
            const SDL_Rect r = menuRowRect(index, kCount);
            return mouseX >= r.x && mouseX < r.x + r.w &&
                   mouseY >= r.y && mouseY < r.y + r.h;
        };
        if (hit(0)) {
            m_running = false;
        } else if (hit(1)) {
            m_mainMenuQuitConfirm = false;
        }
        return;
    }

    constexpr int kCount = 3;  // Start Game, Settings, Quit Game
    auto hit = [&](int index) {
        const SDL_Rect r = menuRowRect(index, kCount);
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };

    if (hit(0)) {
        startGameFromMenu();
    } else if (hit(1)) {
        m_pauseScreen = PauseScreen::Settings;
        m_rebindingAction = -1;
        m_openDropdown = Dropdown::None;
    } else if (hit(2)) {
        m_mainMenuQuitConfirm = true;
    }
}

void Game::startGameFromMenu() {
    if (m_player) {
        float px = m_player->x();
        float py = m_player->y();
        m_rooms.resetToHub(px, py, m_player->size());
        m_player->setPosition(px, py);
    }
    m_enemies.clear();
    m_mainMenuOpen = false;
    m_paused = false;
    m_spellbookOpen = false;
    m_runPromptOpen = false;
    m_runPromptDismissed = false;
    m_mainMenuQuitConfirm = false;
    m_pauseScreen = PauseScreen::Main;
    m_rebindingAction = -1;
    m_openDropdown = Dropdown::None;
    updateCamera();
}

void Game::acceptRunPrompt() {
    if (!m_player) {
        return;
    }
    float px = m_player->x();
    float py = m_player->y();
    m_rooms.startRun(px, py, m_player->size());
    m_player->setPosition(px, py);
    m_enemies.spawnRandom(m_rooms.interiorRect(),
                          px + m_player->size() * 0.5f,
                          py + m_player->size() * 0.5f,
                          10);
    m_runPromptOpen = false;
    m_runPromptDismissed = false;
    updateCamera();
}

void Game::handleGameEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN: {
            if (m_runPromptOpen) {
                if (event.key.keysym.sym == SDLK_y ||
                    event.key.keysym.sym == SDLK_RETURN ||
                    event.key.keysym.sym == SDLK_KP_ENTER) {
                    acceptRunPrompt();
                } else if (event.key.keysym.sym == SDLK_n ||
                           event.key.keysym.sym == SDLK_ESCAPE) {
                    m_runPromptOpen = false;
                    m_runPromptDismissed = true;
                }
                break;
            }
            const SDL_Scancode sc = event.key.keysym.scancode;
            // Esc opens the pause menu; quitting now happens through the menu's
            // confirmed Quit Game button.
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                m_paused = true;
                m_pauseScreen = PauseScreen::Main;
                m_rebindingAction = -1;
            } else if (sc == m_keybinds.get(Action::Spellbook)) {
                m_spellbookOpen = true;
            } else if (event.key.repeat == 0 && m_player) {
                // One-shot keyboard actions (roll/use); movement is polled.
                if (sc == m_keybinds.get(Action::Roll)) {
                    m_player->roll();
                } else if (sc == m_keybinds.get(Action::UseItem)) {
                    m_player->useItem();
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            // Left mouse button swings the melee attack.
            if (event.button.button == SDL_BUTTON_LEFT && m_player) {
                if (m_player->isCasting()) {
                    m_player->appendCastInput(CastInput::Left);
                } else {
                    m_player->attack();
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT && m_player) {
                if (m_player->isCasting()) {
                    m_player->appendCastInput(CastInput::Right);
                }
            } else if (event.button.button == SDL_BUTTON_MIDDLE && m_player) {
                if (m_player->isCasting()) {
                    // Map the real click position into logical (world) space.
                    const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                    m_player->castSpell(static_cast<float>(lp.x) + m_cameraX,
                                        static_cast<float>(lp.y) + m_cameraY);
                } else {
                    m_player->beginCasting();
                }
            }
            break;
        default:
            break;
    }
}

void Game::handlePauseEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN: {
            const SDL_Scancode sc = event.key.keysym.scancode;
            // While listening for a rebind, the next key becomes the binding.
            // Escape cancels the rebind instead of capturing it.
            if (m_rebindingAction >= 0) {
                if (event.key.keysym.sym != SDLK_ESCAPE) {
                    m_keybinds.set(static_cast<Action>(m_rebindingAction), sc);
                    m_keybinds.save();  // persist the change immediately
                }
                m_rebindingAction = -1;
                break;
            }
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                // Back out one level: close an open dropdown first, then a
                // sub-screen returns to Main, and Esc on Main resumes the game.
                if (m_openDropdown != Dropdown::None) {
                    m_openDropdown = Dropdown::None;
                } else if (m_pauseScreen == PauseScreen::Settings ||
                           m_pauseScreen == PauseScreen::ConfirmQuit) {
                    m_pauseScreen = PauseScreen::Main;
                } else {
                    m_paused = false;
                }
            }
            break;
        }
        case SDL_MOUSEWHEEL:
            // Scroll the resolution dropdown when it is open.
            if (m_openDropdown == Dropdown::Resolution) {
                m_resScroll -= event.wheel.y;
                const int maxScroll = kResolutionCount - kResVisible;
                if (m_resScroll < 0) m_resScroll = 0;
                if (m_resScroll > maxScroll) m_resScroll = maxScroll;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                // Map the real click position to the logical space the menu rows
                // are laid out in, then hit-test.
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                handlePauseClick(lp.x, lp.y);
            }
            break;
        default:
            break;
    }
}

void Game::handlePauseClick(int mouseX, int mouseY) {
    // A click while listening for a rebind is ignored; rebinds want a key.
    if (m_rebindingAction >= 0) {
        return;
    }

    auto hit = [&](int index, int count) {
        const SDL_Rect r = menuRowRect(index, count);
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };

    if (m_pauseScreen == PauseScreen::Main) {
        constexpr int kCount = 3;  // Resume, Settings, Quit Game
        if (hit(0, kCount)) {
            m_paused = false;
        } else if (hit(1, kCount)) {
            m_pauseScreen = PauseScreen::Settings;
        } else if (hit(2, kCount)) {
            m_pauseScreen = PauseScreen::ConfirmQuit;
        }
        return;
    }

    if (m_pauseScreen == PauseScreen::ConfirmQuit) {
        constexpr int kCount = 3;  // Quit to Main Menu, Quit to Desktop, Cancel
        if (hit(0, kCount)) {
            m_mainMenuOpen = true;
            m_paused = false;
            m_pauseScreen = PauseScreen::Main;
            m_spellbookOpen = false;
            m_rebindingAction = -1;
            m_openDropdown = Dropdown::None;
        } else if (hit(1, kCount)) {
            m_running = false;  // confirmed: end the game loop
        } else if (hit(2, kCount)) {
            m_pauseScreen = PauseScreen::Main;
        }
        return;
    }

    // Settings: tabs first, then the active page's rows/dropdowns.
    const int actionCount = static_cast<int>(Action::Count);
    const int rowCount = settingsRowCount();

    auto inRect = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };

    for (int i = 0; i < kSettingsTabCount; ++i) {
        if (inRect(settingsTabRect(i))) {
            m_settingsTab = static_cast<SettingsTab>(i);
            m_openDropdown = Dropdown::None;
            m_rebindingAction = -1;
            return;
        }
    }

    // An open dropdown captures the click first.
    if (m_openDropdown == Dropdown::DisplayMode) {
        for (int i = 0; i < kDisplayModeCount; ++i) {
            if (inRect(displayModeOptionRect(i))) {
                setDisplayMode(i);
                m_openDropdown = Dropdown::None;
                return;
            }
        }
        m_openDropdown = Dropdown::None;  // clicked elsewhere: close it
        return;
    }
    if (m_openDropdown == Dropdown::Resolution) {
        for (int slot = 0; slot < kResVisible; ++slot) {
            const int idx = m_resScroll + slot;
            if (idx >= kResolutionCount) break;
            if (inRect(resolutionOptionRect(slot))) {
                applyResolution(idx);
                m_openDropdown = Dropdown::None;
                return;
            }
        }
        m_openDropdown = Dropdown::None;
        return;
    }

    auto settingsHit = [&](int index) {
        const SDL_Rect r = settingsRowRect(index);
        return inRect(r);
    };

    if (m_settingsTab == SettingsTab::Video) {
        if (settingsHit(0)) {
            m_openDropdown = Dropdown::DisplayMode;
            return;
        }
        if (settingsHit(1)) {
            m_openDropdown = Dropdown::Resolution;
            // Scroll so the current selection is visible.
            const int maxScroll = kResolutionCount - kResVisible;
            m_resScroll = m_resIndex - kResVisible / 2;
            if (m_resScroll < 0) m_resScroll = 0;
            if (m_resScroll > maxScroll) m_resScroll = maxScroll;
            return;
        }
        if (settingsHit(2)) {
            m_vsync = !m_vsync;
            recreateRenderer();
            saveVideoSettings();
            return;
        }
        if (settingsHit(3)) {
            m_frameLimitIndex = (m_frameLimitIndex + 1) % kFrameLimitCount;
            if (kFrameLimitValues[m_frameLimitIndex] == 0) {
                m_vsync = false;
                recreateRenderer();
            }
            saveVideoSettings();
            return;
        }
        if (settingsHit(4)) {
            m_integerScaling = !m_integerScaling;
            saveVideoSettings();
            return;
        }
        if (settingsHit(5)) {
            m_showFps = !m_showFps;
            saveVideoSettings();
            return;
        }
        if (settingsHit(6)) {
            m_vsync = true;
            m_frameLimitIndex = 2;
            m_integerScaling = false;
            m_showFps = false;
            recreateRenderer();
            saveVideoSettings();
            return;
        }
        if (settingsHit(7)) {
            m_openDropdown = Dropdown::None;
            m_pauseScreen = PauseScreen::Main;
            return;
        }
    } else if (m_settingsTab == SettingsTab::Audio) {
        if (settingsHit(0)) {
            stepPercent(m_masterVolume);
            saveVideoSettings();
            return;
        }
        if (settingsHit(1)) {
            stepPercent(m_musicVolume);
            saveVideoSettings();
            return;
        }
        if (settingsHit(2)) {
            stepPercent(m_sfxVolume);
            saveVideoSettings();
            return;
        }
        if (settingsHit(3)) {
            m_muteWhenUnfocused = !m_muteWhenUnfocused;
            saveVideoSettings();
            return;
        }
        if (settingsHit(4)) {
            m_masterVolume = 100;
            m_musicVolume = 80;
            m_sfxVolume = 80;
            m_muteWhenUnfocused = false;
            saveVideoSettings();
            return;
        }
        if (settingsHit(5)) {
            m_pauseScreen = PauseScreen::Main;
            return;
        }
    } else if (m_settingsTab == SettingsTab::Controls) {
        for (int i = 0; i < actionCount; ++i) {
            if (settingsHit(i)) {
                m_rebindingAction = i;
                return;
            }
        }
        if (settingsHit(actionCount)) {
            m_keybinds.resetDefaults();
            m_keybinds.save();
        } else if (settingsHit(actionCount + 1)) {
            m_pauseScreen = PauseScreen::Main;
        }
    } else if (m_settingsTab == SettingsTab::Gameplay) {
        if (settingsHit(0)) {
            m_pauseOnFocusLoss = !m_pauseOnFocusLoss;
            saveVideoSettings();
            return;
        }
        if (settingsHit(1)) {
            m_customCursor = !m_customCursor;
            SDL_ShowCursor(m_customCursor ? SDL_DISABLE : SDL_ENABLE);
            saveVideoSettings();
            return;
        }
        if (settingsHit(2)) {
            m_tutorialHints = !m_tutorialHints;
            saveVideoSettings();
            return;
        }
        if (settingsHit(3)) {
            m_pauseOnFocusLoss = true;
            m_customCursor = true;
            m_tutorialHints = true;
            SDL_ShowCursor(SDL_DISABLE);
            saveVideoSettings();
            return;
        }
        if (settingsHit(4)) {
            m_pauseScreen = PauseScreen::Main;
            return;
        }
    }
}

void Game::handleSpellbookEvent(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) {
        return;
    }
    // Esc, or the Spellbook key again, closes the overlay.
    if (event.key.keysym.sym == SDLK_ESCAPE ||
        event.key.keysym.scancode == m_keybinds.get(Action::Spellbook)) {
        m_spellbookOpen = false;
    }
}

void Game::update(float deltaSeconds) {
    if (m_player) {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        m_player->update(deltaSeconds, keys, m_keybinds);

        // Resolve wall collisions and door transitions against the active room.
        float px = m_player->x();
        float py = m_player->y();
        m_rooms.resolvePlayer(px, py, m_player->size());
        m_player->setPosition(px, py);
        const float playerCenterX = px + m_player->size() * 0.5f;
        const float playerCenterY = py + m_player->size() * 0.5f;
        if (!m_rooms.isHub()) {
            m_enemies.update(deltaSeconds, m_rooms.interiorRect(),
                             playerCenterX, playerCenterY);

            SDL_Rect hitbox{};
            if (m_player->attackHitbox(&hitbox)) {
                m_enemies.damageInRect(hitbox, 1);
            }

            float spellX = 0.0f;
            float spellY = 0.0f;
            float spellRadius = 0.0f;
            if (m_player->activeSpellCircle(&spellX, &spellY, &spellRadius) &&
                m_enemies.damageCircle(spellX, spellY, spellRadius, 2)) {
                m_player->clearActiveSpell();
            }
        } else {
            m_enemies.clear();
        }
        const bool overRunDoor =
            m_rooms.isHub() && m_rooms.playerInRunDoor(px, py, m_player->size());
        if (overRunDoor) {
            if (!m_runPromptDismissed) {
                m_runPromptOpen = true;
            }
        } else {
            m_runPromptOpen = false;
            m_runPromptDismissed = false;
        }
        updateCamera();
    }
}

void Game::updateCamera() {
    if (!m_player) {
        m_cameraX = 0.0f;
        m_cameraY = 0.0f;
        return;
    }

    const SDL_Rect room = m_rooms.interiorRect();
    const float playerCenterX = m_player->x() + m_player->size() * 0.5f;
    const float playerCenterY = m_player->y() + m_player->size() * 0.5f;

    auto cameraAxis = [](float center, int viewportSize, int minWorld, int worldSize) {
        if (worldSize <= viewportSize) {
            return static_cast<float>(minWorld) -
                   (viewportSize - worldSize) * 0.5f;
        }
        const float minCamera = static_cast<float>(minWorld);
        const float maxCamera = static_cast<float>(minWorld + worldSize - viewportSize);
        float camera = center - viewportSize * 0.5f;
        if (camera < minCamera) camera = minCamera;
        if (camera > maxCamera) camera = maxCamera;
        return camera;
    };

    m_cameraX = cameraAxis(playerCenterX, m_width,
                           room.x - kCameraBoundaryBuffer,
                           room.w + kCameraBoundaryBuffer * 2);
    m_cameraY = cameraAxis(playerCenterY, m_height,
                           room.y - kCameraBoundaryBuffer,
                           room.h + kCameraBoundaryBuffer * 2);
}

void Game::render() {
    beginFrame();
    // Clear to the "void" outside the room, then draw only the current room.
    // Because the whole window is repainted each frame, neighboring rooms are
    // never visible at the same time.
    SDL_SetRenderDrawColor(m_renderer, 10, 10, 12, 255);
    SDL_RenderClear(m_renderer);
    SDL_Texture* vendorTexture =
        m_textures ? m_textures->defaultFor(AssetKind::Vendor) : nullptr;
    m_rooms.render(m_renderer, m_cameraX, m_cameraY, vendorTexture);
    if (!m_rooms.isHub()) {
        m_enemies.render(m_renderer, m_cameraX, m_cameraY);
    }

    // The player draws itself (sprite, melee hitbox, item effect).
    if (m_player) {
        m_player->render(m_renderer, m_cameraX, m_cameraY);
    }

    SDL_Color color = {255, 255, 255, 255}; // RGBA
    SDL_Surface* surface = TTF_RenderText_Blended(m_font, "Fireball: < >", color);  
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface); // free immediately, you don't need it anymore
    int w, h;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
    SDL_Rect dst = { 10, 10, w, h };
    SDL_RenderCopy(m_renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture); // free after rendering

    renderRunPrompt();
    
    drawCursor();
    presentFrame();
}

void Game::shutdown() {
    // Textures must be freed before the renderer that created them.
    m_player.reset();
    m_textures.reset();
    if (m_canvas) {
        SDL_DestroyTexture(m_canvas);
        m_canvas = nullptr;
    }

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    TTF_Quit();
    // SDL_Quit is idempotent; calling it after a partial init is fine.
    SDL_Quit();
    m_running = false;
}

void Game::drawCursor() {
    if (!m_customCursor) {
        return;
    }
    // Draw in logical space so the crosshair lines up with menus and gameplay
    // regardless of the window resolution.
    const SDL_Point m = mouseLogical();
    const int mx = m.x;
    const int my = m.y;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // A crosshair: four ticks around a small gap, plus a center dot. The mouse
    // position is the hotspot, so it sits exactly at the crosshair's center.
    constexpr int kGap = 4;
    constexpr int kLen = 10;

    // Soft dark backing for contrast against bright backgrounds.
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 150);
    SDL_RenderDrawLine(m_renderer, mx - kGap - kLen, my + 1, mx - kGap, my + 1);
    SDL_RenderDrawLine(m_renderer, mx + kGap, my + 1, mx + kGap + kLen, my + 1);
    SDL_RenderDrawLine(m_renderer, mx + 1, my - kGap - kLen, mx + 1, my - kGap);
    SDL_RenderDrawLine(m_renderer, mx + 1, my + kGap, mx + 1, my + kGap + kLen);

    SDL_SetRenderDrawColor(m_renderer, 240, 240, 245, 235);
    SDL_RenderDrawLine(m_renderer, mx - kGap - kLen, my, mx - kGap, my);
    SDL_RenderDrawLine(m_renderer, mx + kGap, my, mx + kGap + kLen, my);
    SDL_RenderDrawLine(m_renderer, mx, my - kGap - kLen, mx, my - kGap);
    SDL_RenderDrawLine(m_renderer, mx, my + kGap, mx, my + kGap + kLen);

    const SDL_Rect dot{mx - 1, my - 1, 3, 3};
    SDL_RenderFillRect(m_renderer, &dot);
}

void Game::drawFpsOverlay() {
    if (!m_showFps) {
        return;
    }
    char text[32];
    std::snprintf(text, sizeof(text), "FPS: %d", m_currentFps);
    drawTextCentered(text, m_width - 58, 24, SDL_Color{170, 255, 170, 255});
}

void Game::renderRunPrompt() {
    if (!m_runPromptOpen) {
        return;
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    const SDL_Rect panel{m_width / 2 - 300, m_height - 150, 600, 100};
    SDL_SetRenderDrawColor(m_renderer, 10, 12, 18, 220);
    SDL_RenderFillRect(m_renderer, &panel);
    SDL_SetRenderDrawColor(m_renderer, 120, 210, 255, 255);
    SDL_RenderDrawRect(m_renderer, &panel);

    drawTextCentered("Start a run?",
                     panel.x + panel.w / 2, panel.y + 32,
                     SDL_Color{255, 255, 255, 255});
    drawTextCentered("Press Y / Enter to begin, or N / Esc to cancel.",
                     panel.x + panel.w / 2, panel.y + 68,
                     SDL_Color{190, 205, 220, 255});
}

SDL_Rect Game::drawTextCentered(const char* text, int cx, int cy, SDL_Color color) {
    SDL_Rect dst = {cx, cy, 0, 0};
    if (!m_font || !text || !*text) {
        return dst;
    }
    SDL_Surface* surface = TTF_RenderText_Blended(m_font, text, color);
    if (!surface) {
        return dst;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    const int w = surface->w;
    const int h = surface->h;
    SDL_FreeSurface(surface);
    dst = SDL_Rect{cx - w / 2, cy - h / 2, w, h};
    SDL_RenderCopy(m_renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    return dst;
}

SDL_Rect Game::menuRowRect(int index, int count) const {
    constexpr int kRowW    = 560;
    constexpr int kRowH    = 40;
    constexpr int kSpacing = 10;
    const int totalH = count * kRowH + (count - 1) * kSpacing;
    // Title sits above the rows, so push the stack a little below center.
    const int startY = m_height / 2 - totalH / 2 + 50;
    return SDL_Rect{
        m_width / 2 - kRowW / 2,
        startY + index * (kRowH + kSpacing),
        kRowW,
        kRowH};
}

void Game::applyVideoMode(bool persist) {
    const Resolution& res = kResolutions[m_resIndex];

    // Leave any fullscreen state first so size/border changes take effect.
    SDL_SetWindowFullscreen(m_window, 0);

    switch (m_displayMode) {
        case DisplayMode::Windowed:
        case DisplayMode::BorderlessWindowed: {
            // Never make the window larger than the display's usable area (the
            // desktop minus the taskbar). A window bigger than the screen hangs
            // off the edges: its title bar goes off-screen (so it can't be
            // interacted with normally) and the centered logical render lands
            // partly in the off-screen region — which is what made the player
            // vanish and clicks mis-map at high resolutions.
            int w = res.w;
            int h = res.h;
            int display = SDL_GetWindowDisplayIndex(m_window);
            if (display < 0) display = 0;
            SDL_Rect usable{};
            if (SDL_GetDisplayUsableBounds(display, &usable) == 0 &&
                usable.w > 0 && usable.h > 0) {
                if (w > usable.w) w = usable.w;
                if (h > usable.h) h = usable.h;
            }
            SDL_SetWindowBordered(
                m_window,
                m_displayMode == DisplayMode::Windowed ? SDL_TRUE : SDL_FALSE);
            SDL_SetWindowSize(m_window, w, h);
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED);
            break;
        }
        case DisplayMode::Fullscreen:
        case DisplayMode::BorderlessFullscreen:
            // Fill the whole display at its current (native) resolution without
            // performing a video-mode switch. Exclusive fullscreen
            // (SDL_WINDOW_FULLSCREEN) would change the monitor's mode, which on
            // Windows rearranges every other window; desktop fullscreen avoids
            // that. The logical render size scales up to fill the screen.
            SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
    }

    // Nothing else to do: the canvas is a fixed size and presentFrame() fits it
    // to whatever the window is now, recomputed fresh every frame.
    if (persist) saveVideoSettings();
}

void Game::applyResolution(int index) {
    if (index < 0 || index >= kResolutionCount) return;
    m_resIndex = index;
    applyVideoMode();
}

void Game::applyVSync() {
    if (m_renderer) {
        SDL_RenderSetVSync(m_renderer, m_vsync ? 1 : 0);
    }
}

bool Game::recreateRenderer() {
    if (!m_window) {
        return false;
    }

    if (m_textures) {
        m_textures->destroy();
    }
    if (m_canvas) {
        SDL_DestroyTexture(m_canvas);
        m_canvas = nullptr;
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    SDL_SetHint(SDL_HINT_RENDER_VSYNC, m_vsync ? "1" : "0");
    const Uint32 flags = SDL_RENDERER_ACCELERATED |
        (m_vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    m_renderer = SDL_CreateRenderer(m_window, -1, flags);
    if (!m_renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    m_canvas = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        m_width,
        m_height);
    if (!m_canvas) {
        std::fprintf(stderr, "SDL_CreateTexture (canvas) failed: %s\n", SDL_GetError());
        return false;
    }

    if (m_textures) {
        m_textures->init(m_renderer, m_width, m_height);
    }
    return true;
}

void Game::setDisplayMode(int mode) {
    if (mode < 0 || mode >= kDisplayModeCount) return;
    m_displayMode = static_cast<DisplayMode>(mode);
    applyVideoMode();
}

void Game::beginFrame() {
    // Direct all drawing into the fixed-size canvas. SDL resets the viewport to
    // the full texture at scale 1, so everything draws in logical coordinates.
    SDL_SetRenderTarget(m_renderer, m_canvas);
}

void Game::presentFrame() {
    // Back to the window, then scale the canvas into it with black letterbox
    // bars around the fitted area.
    drawFpsOverlay();
    SDL_SetRenderTarget(m_renderer, nullptr);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    const SDL_Rect dst = canvasRect();
    SDL_RenderCopy(m_renderer, m_canvas, nullptr, &dst);
    SDL_RenderPresent(m_renderer);
}

SDL_Rect Game::canvasRect() const {
    const SDL_Point output = windowOutputSize();
    const int outW = output.x;
    const int outH = output.y;
    // Largest integer-pixel rectangle of logical aspect that fits the output.
    const float scaleX = static_cast<float>(outW) / m_width;
    const float scaleY = static_cast<float>(outH) / m_height;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (m_integerScaling && scale >= 1.0f) {
        scale = static_cast<float>(static_cast<int>(scale));
    }
    const int w = static_cast<int>(m_width  * scale);
    const int h = static_cast<int>(m_height * scale);
    return SDL_Rect{(outW - w) / 2, (outH - h) / 2, w, h};
}

SDL_Point Game::windowOutputSize() const {
    int outW = 0, outH = 0;
    SDL_GetWindowSizeInPixels(m_window, &outW, &outH);
    if (outW <= 0 || outH <= 0) {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(m_window, &winW, &winH);
        outW = winW;
        outH = winH;
    }
    if (outW <= 0 || outH <= 0) {
        outW = m_width;
        outH = m_height;
    }
    return SDL_Point{outW, outH};
}

SDL_Point Game::windowToLogical(int windowX, int windowY) const {
    const SDL_Point output = windowOutputSize();
    const int outW = output.x;
    const int outH = output.y;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(m_window, &winW, &winH);
    if (winW <= 0) winW = outW;
    if (winH <= 0) winH = outH;

    // Window points -> renderer output pixels (1:1 unless high-DPI).
    const float px = windowX * static_cast<float>(outW) / winW;
    const float py = windowY * static_cast<float>(outH) / winH;

    // Invert the exact blit rectangle used to present the canvas: output pixels
    // inside dst map linearly back to [0, m_width) x [0, m_height).
    const SDL_Rect dst = canvasRect();
    const int lx = dst.w > 0 ? static_cast<int>((px - dst.x) * m_width  / dst.w) : 0;
    const int ly = dst.h > 0 ? static_cast<int>((py - dst.y) * m_height / dst.h) : 0;
    return SDL_Point{lx, ly};
}

SDL_Point Game::mouseLogical() const {
    int wx = 0, wy = 0;
    SDL_GetMouseState(&wx, &wy);
    return windowToLogical(wx, wy);
}

int Game::settingsRowCount() const {
    switch (m_settingsTab) {
        case SettingsTab::Video:
            return 8;  // display, resolution, vsync, frame cap, scaling, FPS, reset, back
        case SettingsTab::Audio:
            return 6;  // master, music, SFX, mute behavior, reset, back
        case SettingsTab::Controls:
            return static_cast<int>(Action::Count) + 2;  // binds, reset, back
        case SettingsTab::Gameplay:
            return 5;  // focus pause, cursor, hints, reset, back
    }
    return 0;
}

SDL_Rect Game::settingsTabRect(int index) const {
    constexpr int kTabW = 150;
    constexpr int kTabH = 36;
    constexpr int kGap = 10;
    const int totalW = kSettingsTabCount * kTabW + (kSettingsTabCount - 1) * kGap;
    return SDL_Rect{
        m_width / 2 - totalW / 2 + index * (kTabW + kGap),
        104,
        kTabW,
        kTabH};
}

SDL_Rect Game::settingsRowRect(int index) const {
    constexpr int kRowW = 720;
    constexpr int kRowH = 38;
    constexpr int kSpacing = 8;
    return SDL_Rect{
        m_width / 2 - kRowW / 2,
        160 + index * (kRowH + kSpacing),
        kRowW,
        kRowH};
}

SDL_Rect Game::displayModeOptionRect(int index) const {
    const SDL_Rect base = settingsRowRect(0);
    constexpr int kOptH = 32;
    return SDL_Rect{base.x, base.y + base.h + index * kOptH, base.w, kOptH};
}

SDL_Rect Game::resolutionOptionRect(int slot) const {
    const SDL_Rect base = settingsRowRect(1);
    constexpr int kOptH = 32;
    return SDL_Rect{base.x, base.y + base.h + slot * kOptH, base.w, kOptH};
}

void Game::loadVideoSettings() {
    m_resIndex = 1;                          // default: 1280 x 720
    m_displayMode = DisplayMode::Windowed;   // default: windowed
    m_vsync = true;
    m_integerScaling = false;
    m_showFps = false;
    m_frameLimitIndex = 2;                    // default: 60 FPS cap if VSync is off
    m_masterVolume = 100;
    m_musicVolume = 80;
    m_sfxVolume = 80;
    m_muteWhenUnfocused = false;
    m_pauseOnFocusLoss = true;
    m_customCursor = true;
    m_tutorialHints = true;
    char* base = SDL_GetPrefPath("Gaia", "Gaia");
    if (!base) return;
    const std::string path = std::string(base) + "video.cfg";
    SDL_free(base);
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return;
    char line[128];
    if (std::fgets(line, sizeof(line), f)) {
        int w = 0, h = 0, mode = 0;
        if (std::sscanf(line, "%d %d %d", &w, &h, &mode) >= 2) {
            for (int i = 0; i < kResolutionCount; ++i) {
                if (kResolutions[i].w == w && kResolutions[i].h == h) {
                    m_resIndex = i;
                    break;
                }
            }
            if (mode >= 0 && mode < kDisplayModeCount) {
                m_displayMode = static_cast<DisplayMode>(mode);
            }
            std::fclose(f);
            return;
        }
        std::rewind(f);
    }

    char key[64];
    int value = 0;
    while (std::fscanf(f, "%63s %d", key, &value) == 2) {
        if (std::strcmp(key, "width") == 0) {
            int h = kResolutions[m_resIndex].h;
            long pos = std::ftell(f);
            char nextKey[64];
            int nextValue = 0;
            if (std::fscanf(f, "%63s %d", nextKey, &nextValue) == 2 &&
                std::strcmp(nextKey, "height") == 0) {
                h = nextValue;
            } else {
                std::fseek(f, pos, SEEK_SET);
            }
            for (int i = 0; i < kResolutionCount; ++i) {
                if (kResolutions[i].w == value && kResolutions[i].h == h) {
                    m_resIndex = i;
                    break;
                }
            }
        } else if (std::strcmp(key, "displayMode") == 0 &&
                   value >= 0 && value < kDisplayModeCount) {
            m_displayMode = static_cast<DisplayMode>(value);
        } else if (std::strcmp(key, "vsync") == 0) {
            m_vsync = value != 0;
        } else if (std::strcmp(key, "integerScaling") == 0) {
            m_integerScaling = value != 0;
        } else if (std::strcmp(key, "showFps") == 0) {
            m_showFps = value != 0;
        } else if (std::strcmp(key, "frameLimit") == 0 &&
                   value >= 0 && value < kFrameLimitCount) {
            m_frameLimitIndex = value;
        } else if (std::strcmp(key, "masterVolume") == 0) {
            m_masterVolume = clampPercent(value);
        } else if (std::strcmp(key, "musicVolume") == 0) {
            m_musicVolume = clampPercent(value);
        } else if (std::strcmp(key, "sfxVolume") == 0) {
            m_sfxVolume = clampPercent(value);
        } else if (std::strcmp(key, "muteWhenUnfocused") == 0) {
            m_muteWhenUnfocused = value != 0;
        } else if (std::strcmp(key, "pauseOnFocusLoss") == 0) {
            m_pauseOnFocusLoss = value != 0;
        } else if (std::strcmp(key, "customCursor") == 0) {
            m_customCursor = value != 0;
        } else if (std::strcmp(key, "tutorialHints") == 0) {
            m_tutorialHints = value != 0;
        }
    }
    if (kFrameLimitValues[m_frameLimitIndex] == 0) {
        m_vsync = false;
    }
    std::fclose(f);
}

void Game::saveVideoSettings() const {
    char* base = SDL_GetPrefPath("Gaia", "Gaia");
    if (!base) return;
    const std::string path = std::string(base) + "video.cfg";
    SDL_free(base);
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "width %d\n", kResolutions[m_resIndex].w);
    std::fprintf(f, "height %d\n", kResolutions[m_resIndex].h);
    std::fprintf(f, "displayMode %d\n", static_cast<int>(m_displayMode));
    std::fprintf(f, "vsync %d\n", m_vsync ? 1 : 0);
    std::fprintf(f, "integerScaling %d\n", m_integerScaling ? 1 : 0);
    std::fprintf(f, "showFps %d\n", m_showFps ? 1 : 0);
    std::fprintf(f, "frameLimit %d\n", m_frameLimitIndex);
    std::fprintf(f, "masterVolume %d\n", m_masterVolume);
    std::fprintf(f, "musicVolume %d\n", m_musicVolume);
    std::fprintf(f, "sfxVolume %d\n", m_sfxVolume);
    std::fprintf(f, "muteWhenUnfocused %d\n", m_muteWhenUnfocused ? 1 : 0);
    std::fprintf(f, "pauseOnFocusLoss %d\n", m_pauseOnFocusLoss ? 1 : 0);
    std::fprintf(f, "customCursor %d\n", m_customCursor ? 1 : 0);
    std::fprintf(f, "tutorialHints %d\n", m_tutorialHints ? 1 : 0);
    std::fclose(f);
}

// Draws a clickable row: a filled box (brighter when hovered) plus its label.
static void drawRow(SDL_Renderer* renderer, const SDL_Rect& row, bool hovered) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (hovered) {
        SDL_SetRenderDrawColor(renderer, 70, 80, 100, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 40, 44, 54, 255);
    }
    SDL_RenderFillRect(renderer, &row);
    SDL_SetRenderDrawColor(renderer, 90, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &row);
}

void Game::renderMainMenu() {
    if (m_pauseScreen == PauseScreen::Settings) {
        renderSettingsMenu();
        return;
    }

    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 8, 10, 14, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color title = {255, 245, 210, 255};
    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color hint  = {170, 175, 185, 255};

    drawTextCentered("Gaia", m_width / 2, m_height / 2 - 190, title);
    drawTextCentered(m_mainMenuQuitConfirm
                         ? "Confirm before leaving Gaia."
                         : "Press Enter or click Start Game",
                     m_width / 2, m_height / 2 - 150, hint);

    const SDL_Point mlp = mouseLogical();
    const int mouseX = mlp.x, mouseY = mlp.y;
    auto hovered = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };

    if (m_mainMenuQuitConfirm) {
        constexpr int kCount = 2;
        const char* labels[kCount] = {"Quit to Desktop", "Cancel"};
        drawTextCentered("Quit Game?", m_width / 2, m_height / 2 - 95, white);
        for (int i = 0; i < kCount; ++i) {
            const SDL_Rect row = menuRowRect(i, kCount);
            drawRow(m_renderer, row, hovered(row));
            drawTextCentered(labels[i], row.x + row.w / 2, row.y + row.h / 2, white);
        }
    } else {
        constexpr int kCount = 3;
        const char* labels[kCount] = {"Start Game", "Settings", "Quit Game"};
        for (int i = 0; i < kCount; ++i) {
            const SDL_Rect row = menuRowRect(i, kCount);
            drawRow(m_renderer, row, hovered(row));
            drawTextCentered(labels[i], row.x + row.w / 2, row.y + row.h / 2, white);
        }
    }

    drawCursor();
    presentFrame();
}

void Game::renderPauseMenu() {
    if (m_pauseScreen == PauseScreen::Settings) {
        renderSettingsMenu();
        return;
    }
    if (m_pauseScreen == PauseScreen::ConfirmQuit) {
        renderConfirmQuitMenu();
        return;
    }

    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 12, 14, 18, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white = {255, 255, 255, 255};
    drawTextCentered("Game Paused", m_width / 2, m_height / 2 - 140, white);

    const SDL_Point mlp = mouseLogical();
    const int mouseX = mlp.x, mouseY = mlp.y;
    auto hovered = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };

    constexpr int kCount = 3;
    const char* labels[kCount] = {"Resume", "Settings", "Quit Game"};
    for (int i = 0; i < kCount; ++i) {
        const SDL_Rect row = menuRowRect(i, kCount);
        drawRow(m_renderer, row, hovered(row));
        drawTextCentered(labels[i], row.x + row.w / 2, row.y + row.h / 2, white);
    }

    drawCursor();
    presentFrame();
}

void Game::renderConfirmQuitMenu() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 12, 14, 18, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white = {255, 255, 255, 255};
    drawTextCentered("Quit Game?", m_width / 2, m_height / 2 - 140, white);
    drawTextCentered("Any unsaved progress will be lost.",
                     m_width / 2, m_height / 2 - 110,
                     SDL_Color{170, 175, 185, 255});

    const SDL_Point mlp = mouseLogical();
    const int mouseX = mlp.x, mouseY = mlp.y;
    auto hovered = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };

    constexpr int kCount = 3;
    const char* labels[kCount] = {"Quit to Main Menu", "Quit to Desktop", "Cancel"};
    for (int i = 0; i < kCount; ++i) {
        const SDL_Rect row = menuRowRect(i, kCount);
        drawRow(m_renderer, row, hovered(row));
        drawTextCentered(labels[i], row.x + row.w / 2, row.y + row.h / 2, white);
    }

    drawCursor();
    presentFrame();
}

void Game::renderSpellbook() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 12, 14, 18, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color hint  = {170, 175, 185, 255};
    const SDL_Color combo = {255, 210, 90, 255};

    drawTextCentered("Spellbook", m_width / 2, m_height / 2 - 230, white);
    drawTextCentered("Middle Mouse begins a cast, enter the combo, then Middle Mouse releases it.",
                     m_width / 2, m_height / 2 - 200, hint);

    const std::vector<SpellDef>& spells = spellRegistry();
    const int rowCount = static_cast<int>(spells.size());
    for (int i = 0; i < rowCount; ++i) {
        const SpellDef& spell = spells[i];
        const SDL_Rect row = menuRowRect(i, rowCount);
        drawRow(m_renderer, row, false);  // informational, not clickable

        // Spell name on the left, its ordered input combo on the right.
        const int textY = row.y + row.h / 2;
        drawTextCentered(spell.name, row.x + 130, textY, white);

        std::string sequence;
        for (std::size_t s = 0; s < spell.sequence.size(); ++s) {
            if (s != 0) sequence += "  ->  ";
            sequence += castInputName(spell.sequence[s]);
        }
        drawTextCentered(sequence.c_str(), row.x + row.w - 200, textY, combo);
    }

    const std::string closeHint =
        std::string("Press Esc or ") + m_keybinds.keyName(Action::Spellbook) +
        " to close.";
    drawTextCentered(closeHint.c_str(), m_width / 2, m_height / 2 + 230, hint);

    drawCursor();
    presentFrame();
}

void Game::renderSettingsMenu() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 12, 14, 18, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color hint  = {170, 175, 185, 255};
    const SDL_Color value = {255, 210, 90, 255};
    drawTextCentered("Settings", m_width / 2, 54, white);
    drawTextCentered("Choose a category. Click rows to cycle values; Esc backs out.",
                     m_width / 2, 82, hint);

    const SDL_Point mp = mouseLogical();
    auto hovered = [&](const SDL_Rect& r) {
        return mp.x >= r.x && mp.x < r.x + r.w &&
               mp.y >= r.y && mp.y < r.y + r.h;
    };

    const int actionCount = static_cast<int>(Action::Count);

    for (int i = 0; i < kSettingsTabCount; ++i) {
        const SDL_Rect tab = settingsTabRect(i);
        const bool active = i == static_cast<int>(m_settingsTab);
        if (hovered(tab))      SDL_SetRenderDrawColor(m_renderer, 70, 80, 100, 255);
        else if (active)       SDL_SetRenderDrawColor(m_renderer, 56, 62, 80, 255);
        else                   SDL_SetRenderDrawColor(m_renderer, 32, 36, 46, 255);
        SDL_RenderFillRect(m_renderer, &tab);
        SDL_SetRenderDrawColor(m_renderer, active ? 255 : 90,
                               active ? 210 : 100,
                               active ? 90 : 120, 255);
        SDL_RenderDrawRect(m_renderer, &tab);
        drawTextCentered(kSettingsTabLabels[i], tab.x + tab.w / 2,
                         tab.y + tab.h / 2, active ? value : white);
    }

    auto drawSetting = [&](int index, const char* label, const char* settingValue) {
        const SDL_Rect row = settingsRowRect(index);
        drawRow(m_renderer, row, hovered(row));
        const int textY = row.y + row.h / 2;
        drawTextCentered(label, row.x + 170, textY, white);
        if (settingValue && *settingValue) {
            drawTextCentered(settingValue, row.x + row.w - 190, textY, value);
        }
    };

    auto drawBack = [&](int index) {
        const SDL_Rect row = settingsRowRect(index);
        drawRow(m_renderer, row, hovered(row));
        drawTextCentered("Back", row.x + row.w / 2, row.y + row.h / 2, white);
    };

    char masterText[16];
    char musicText[16];
    char sfxText[16];
    std::snprintf(masterText, sizeof(masterText), "%d%%", m_masterVolume);
    std::snprintf(musicText, sizeof(musicText), "%d%%", m_musicVolume);
    std::snprintf(sfxText, sizeof(sfxText), "%d%%", m_sfxVolume);

    if (m_settingsTab == SettingsTab::Video) {
        drawSetting(0, "Display Mode", kDisplayModeLabels[static_cast<int>(m_displayMode)]);
        drawSetting(1, "Resolution", kResolutions[m_resIndex].label);
        drawSetting(2, "VSync", onOff(m_vsync));
        drawSetting(3, "Frame Limit", kFrameLimitLabels[m_frameLimitIndex]);
        drawSetting(4, "Integer Pixel Scaling", onOff(m_integerScaling));
        drawSetting(5, "Show FPS", onOff(m_showFps));
        drawSetting(6, "Reset Video Settings", "");
        drawBack(7);
    } else if (m_settingsTab == SettingsTab::Audio) {
        drawSetting(0, "Master Volume", masterText);
        drawSetting(1, "Music Volume", musicText);
        drawSetting(2, "SFX Volume", sfxText);
        drawSetting(3, "Mute When Unfocused", onOff(m_muteWhenUnfocused));
        drawSetting(4, "Reset Audio Settings", "");
        drawBack(5);
    } else if (m_settingsTab == SettingsTab::Controls) {
        for (int i = 0; i < actionCount; ++i) {
            const Action action = static_cast<Action>(i);
            const bool listening = (m_rebindingAction == i);
            drawSetting(i, Keybindings::label(action),
                        listening ? "Press a key..." : m_keybinds.keyName(action));
        }
        drawSetting(actionCount, "Reset Keybindings", "");
        drawBack(actionCount + 1);
    } else if (m_settingsTab == SettingsTab::Gameplay) {
        drawSetting(0, "Pause When Unfocused", onOff(m_pauseOnFocusLoss));
        drawSetting(1, "Custom Pixel Cursor", onOff(m_customCursor));
        drawSetting(2, "Tutorial Hints", onOff(m_tutorialHints));
        drawSetting(3, "Reset Gameplay Settings", "");
        drawBack(4);
    }

    // Expanded dropdowns are drawn last so they overlay the rows beneath them.
    auto drawOption = [&](const SDL_Rect& opt, const char* label, bool current) {
        if (hovered(opt))   SDL_SetRenderDrawColor(m_renderer, 86, 96, 120, 255);
        else if (current)   SDL_SetRenderDrawColor(m_renderer, 56, 62, 80, 255);
        else                SDL_SetRenderDrawColor(m_renderer, 30, 33, 42, 255);
        SDL_RenderFillRect(m_renderer, &opt);
        SDL_SetRenderDrawColor(m_renderer, 90, 100, 120, 255);
        SDL_RenderDrawRect(m_renderer, &opt);
        drawTextCentered(label, opt.x + opt.w / 2, opt.y + opt.h / 2,
                         current ? value : white);
    };

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    if (m_openDropdown == Dropdown::DisplayMode) {
        for (int i = 0; i < kDisplayModeCount; ++i) {
            drawOption(displayModeOptionRect(i), kDisplayModeLabels[i],
                       i == static_cast<int>(m_displayMode));
        }
    } else if (m_openDropdown == Dropdown::Resolution) {
        // Only kResVisible entries show at once; the list scrolls.
        for (int slot = 0; slot < kResVisible; ++slot) {
            const int idx = m_resScroll + slot;
            if (idx >= kResolutionCount) break;
            drawOption(resolutionOptionRect(slot), kResolutions[idx].label,
                       idx == m_resIndex);
        }
        // Scrollbar on the right edge of the list.
        const SDL_Rect first = resolutionOptionRect(0);
        const SDL_Rect last  = resolutionOptionRect(kResVisible - 1);
        const int trackY = first.y;
        const int trackH = (last.y + last.h) - first.y;
        const SDL_Rect track{first.x + first.w - 8, trackY, 6, trackH};
        SDL_SetRenderDrawColor(m_renderer, 24, 26, 33, 255);
        SDL_RenderFillRect(m_renderer, &track);
        const int thumbH = trackH * kResVisible / kResolutionCount;
        const int maxScroll = kResolutionCount - kResVisible;
        const int thumbY = trackY + (maxScroll > 0
                               ? (trackH - thumbH) * m_resScroll / maxScroll : 0);
        const SDL_Rect thumb{track.x, thumbY, track.w, thumbH};
        SDL_SetRenderDrawColor(m_renderer, 120, 130, 150, 255);
        SDL_RenderFillRect(m_renderer, &thumb);
    }

    drawCursor();
    presentFrame();
}

}  // namespace gaia
