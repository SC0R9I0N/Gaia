#include "Game.hpp"

#include "PlaceholderTextures.hpp"
#include "Player.hpp"
#include "SpellCaster.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <utility>

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

constexpr int kArtifactColumns = 2;
constexpr int kArtifactVisibleRows = 3;
constexpr int kArtifactVisibleCount = kArtifactColumns * kArtifactVisibleRows;
constexpr int kRuneLibraryVisibleCount = 10;

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

int clampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

std::vector<int> parseIntList(const std::string& value) {
    std::vector<int> out;
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            out.push_back(std::atoi(token.c_str()));
        }
    }
    return out;
}

std::vector<bool> parseBoolList(const std::string& value) {
    std::vector<bool> out;
    for (int valueInt : parseIntList(value)) {
        out.push_back(valueInt != 0);
    }
    return out;
}

std::string joinInts(const std::vector<int>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out += ",";
        out += std::to_string(values[i]);
    }
    return out;
}

std::string joinBools(const std::vector<bool>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out += ",";
        out += values[i] ? "1" : "0";
    }
    return out;
}

std::string progressSavePath() {
    char* pref = SDL_GetPrefPath("Gaia", "Gaia");
    if (!pref) {
        return "gaia_save.txt";
    }
    std::string path(pref);
    SDL_free(pref);
    path += "gaia_save.txt";
    return path;
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
    m_characterOptions = {{
        {"Astral Wizard", "Balanced arcane vessel", AssetKind::Character, true},
        {"Dust Gunslinger", "A quick-draw revolver shell", AssetKind::Character2, true},
        {"Iron Sentinel", "An armored vanguard vessel", AssetKind::Character3, true},
        {"Veiled Rogue", "A silent blade in the dark", AssetKind::Character4, true},
        {"Wild Ranger", "A keen eye for distant marks", AssetKind::Character5, true},
        {"Unknown Vessel", "Unlock method unknown", AssetKind::Character6, false},
    }};
    m_artifactOptions = {
        {"Gilded Lens", "+25% gold from enemy kills.", "Unlocked by default.",
         SDL_Color{255, 205, 90, 255}, true, -1},
        {"Glass Cannon Sigil", "+Future spell damage. Future downside: fragile body.",
         "Unlocked by default for prototype testing.",
         SDL_Color{255, 105, 125, 255}, true, -1},
        {"Hoarder's Magnet", "+Future pickup range and currency pull.",
         "Challenge: clear a room without leaving gold behind.",
         SDL_Color{110, 220, 255, 255}, false, -1},
        {"Oath of Ash", "+Future revive once per run. Future downside: cursed rooms.",
         "Challenge: defeat a bomber without triggering its blast.",
         SDL_Color{210, 120, 75, 255}, false, -1},
        {"Clockwork Heart", "+Future cooldown recovery. Future downside: slower movement.",
         "Challenge: clear three rooms under a time limit.",
         SDL_Color{185, 165, 255, 255}, false, -1},
        {"Veil of Thorns", "+Future contact retaliation. Future downside: thorned healing.",
         "Challenge: survive a combat room without using dash.",
         SDL_Color{85, 205, 120, 255}, false, -1},
        {"Starved Crown", "+Future elite rewards. Future downside: fewer hearts.",
         "Challenge: defeat an elite while below half health.",
         SDL_Color{240, 190, 80, 255}, false, -1},
        {"Mirror Shard", "+Future duplicate spell echo. Future downside: random misfires.",
         "Challenge: reflect or avoid twenty projectiles in one room.",
         SDL_Color{150, 220, 245, 255}, false, -1},
        {"Frostbound Anklet", "+Future chill aura. Future downside: delayed movement starts.",
         "Challenge: clear a room after freezing every enemy once.",
         SDL_Color{120, 200, 255, 255}, false, -1},
        {"Leeching Quill", "+Future lifesteal. Future downside: lower max skill gains.",
         "Challenge: finish a room with exactly one hit point remaining.",
         SDL_Color{160, 40, 75, 255}, false, -1},
        {"Sunken Bell", "+Future room-clear pulse. Future downside: louder ambushes.",
         "Challenge: discover a hidden cache in the flooded ruins.",
         SDL_Color{70, 150, 185, 255}, false, -1},
        {"Witchglass Compass", "+Future secret-room hints. Future downside: cursed exits.",
         "Challenge: enter three optional rooms in a single run.",
         SDL_Color{190, 110, 235, 255}, false, -1},
        {"Ashen Dice", "+Future rerolls. Future downside: unstable shop prices.",
         "Challenge: buy nothing from two vendors in a row.",
         SDL_Color{210, 105, 85, 255}, false, -1},
        {"Grave Moth Brooch", "+Future death-mark damage. Future downside: dimmer vision.",
         "Challenge: defeat five enemies within three seconds.",
         SDL_Color{175, 175, 210, 255}, false, -1},
        {"Null Lantern", "+Future void shield. Future downside: reduced gold drops.",
         "Challenge: clear a dark room without taking damage.",
         SDL_Color{90, 90, 120, 255}, false, -1},
    };
    m_researchedSpells.assign(allSpellRegistry().size(), false);
    for (std::size_t i = 0; i < m_researchedSpells.size(); ++i) {
        m_researchedSpells[i] = true;
    }

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
    // Pre-create every texture now, outside the render loop. The procedural
    // fallbacks render to an off-screen target; doing it here avoids switching
    // the render target mid-frame and surfaces any PNG load issues at startup.
    for (AssetKind kind : {AssetKind::Character, AssetKind::Character2,
                           AssetKind::Character3, AssetKind::Character4,
                           AssetKind::Character5, AssetKind::Character6,
                           AssetKind::Item,
                           AssetKind::Background, AssetKind::Vendor,
                           AssetKind::RuneVendor, AssetKind::ShadyVendor,
                           AssetKind::ConsciousnessConsole,
                           AssetKind::SkillTreeConsole,
                           AssetKind::ArtifactStorage,
                           AssetKind::RunPortal,
                           AssetKind::GoldCurrency, AssetKind::SkillCurrency,
                           AssetKind::ShadyCurrency,
                           AssetKind::Enemy, AssetKind::Floor,
                           AssetKind::Attack, AssetKind::Dash,
                           AssetKind::Dash2, AssetKind::Dash3,
                           AssetKind::Dash4, AssetKind::Dash5,
                           AssetKind::Dash6,
                           AssetKind::HubFloor, AssetKind::HubWall,
                           AssetKind::HubLantern, AssetKind::HubPillar,
                           AssetKind::HubUrn, AssetKind::HubBanner}) {
        m_textures->defaultFor(kind);
    }

    m_rooms.init(m_width, m_height);

    m_player = std::make_unique<Player>();
    const float half = m_player->size() * 0.5f;
    const SDL_Point spawn = m_rooms.spawnCenter();
    m_player->init(m_textures.get(),
                   static_cast<float>(spawn.x) - half,
                   static_cast<float>(spawn.y) - half);
    m_player->setCharacterKind(selectedCharacter().texture);
    m_playerSafeX = m_player->x();
    m_playerSafeY = m_player->y();
    updateCamera();

    // Hide the OS cursor when the custom pixel cursor is enabled.
    SDL_ShowCursor(m_customCursor ? SDL_DISABLE : SDL_ENABLE);

    // Apply any keybinds the player saved in a previous session.
    m_keybinds.load();
    initializeInventory();
    loadProgress();

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
        } else if (m_runeConfigOpen) {
            renderRuneConfig();
        } else if (m_shadyShopOpen) {
            renderShadyShop();
        } else if (m_consciousnessOpen) {
            renderConsciousnessConsole();
        } else if (m_skillTreeOpen) {
            renderSkillTreeSelector();
        } else if (m_artifactStorageOpen) {
            renderArtifactStorage();
        } else if (m_inventoryOpen) {
            renderInventory();
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
        } else if (m_runeConfigOpen) {
            handleRuneConfigEvent(event);
        } else if (m_shadyShopOpen) {
            handleShadyShopEvent(event);
        } else if (m_consciousnessOpen) {
            handleConsciousnessEvent(event);
        } else if (m_skillTreeOpen) {
            handleSkillTreeEvent(event);
        } else if (m_artifactStorageOpen) {
            handleArtifactStorageEvent(event);
        } else if (m_inventoryOpen) {
            handleInventoryEvent(event);
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
    auto mainMenuRow = [&](int index, int count) {
        constexpr int kRowW = 380;
        constexpr int kRowH = 46;
        constexpr int kSpacing = 14;
        const int totalH = count * kRowH + (count - 1) * kSpacing;
        return SDL_Rect{110, m_height / 2 - totalH / 2 + 84 + index * (kRowH + kSpacing),
                        kRowW, kRowH};
    };

    if (m_mainMenuQuitConfirm) {
        constexpr int kCount = 2;  // Quit to Desktop, Cancel
        auto hit = [&](int index) {
            const SDL_Rect r = mainMenuRow(index, kCount);
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
        const SDL_Rect r = mainMenuRow(index, kCount);
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
        m_playerSafeX = px;
        m_playerSafeY = py;
    }
    m_enemies.clear();
    m_gold = 0;
    setPlayerHealth(m_playerMaxHealth);
    m_playerDamageCooldown = 0.0f;
    m_mainMenuOpen = false;
    m_paused = false;
    m_spellbookOpen = false;
    m_inventoryOpen = false;
    m_runeConfigOpen = false;
    m_shadyShopOpen = false;
    m_consciousnessOpen = false;
    m_skillTreeOpen = false;
    m_artifactStorageOpen = false;
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
    captureRunSnapshot();
    m_gold = 0;
    setPlayerHealth(m_playerMaxHealth);
    m_playerDamageCooldown = 0.0f;
    float px = m_player->x();
    float py = m_player->y();
    m_rooms.startRun(px, py, m_player->size());
    m_player->setPosition(px, py);
    m_playerSafeX = px;
    m_playerSafeY = py;
    // Spawn the chosen layout's hand-placed encounter.
    m_enemies.spawnAt(m_rooms.currentEnemySpawns());
    m_roomClearRewarded = false;
    m_runPromptOpen = false;
    m_runPromptDismissed = false;
    updateCamera();
}

void Game::exitRunToHub() {
    if (!m_player) {
        return;
    }
    restoreRunSnapshot();
    m_gold = 0;
    setPlayerHealth(m_playerMaxHealth);
    m_playerDamageCooldown = 0.0f;
    float px = m_player->x();
    float py = m_player->y();
    m_rooms.resetToHub(px, py, m_player->size());
    m_player->setPosition(px, py);
    m_playerSafeX = px;
    m_playerSafeY = py;
    m_enemies.clear();
    m_paused = false;
    m_spellbookOpen = false;
    m_inventoryOpen = false;
    m_runeConfigOpen = false;
    m_shadyShopOpen = false;
    m_consciousnessOpen = false;
    m_skillTreeOpen = false;
    m_artifactStorageOpen = false;
    m_runPromptOpen = false;
    m_runPromptDismissed = false;
    m_roomClearRewarded = false;
    m_runSnapshot.valid = false;
    m_pauseScreen = PauseScreen::Main;
    m_rebindingAction = -1;
    m_openDropdown = Dropdown::None;
    updateCamera();
}

void Game::handlePlayerDeath() {
    m_gold = 0;
    removeNonRuneInventory();
    setPlayerHealth(m_playerMaxHealth);
    m_playerDamageCooldown = 0.0f;
    m_runSnapshot.valid = false;
    if (m_player) {
        float px = m_player->x();
        float py = m_player->y();
        m_rooms.resetToHub(px, py, m_player->size());
        m_player->setPosition(px, py);
        m_playerSafeX = px;
        m_playerSafeY = py;
    }
    m_enemies.clear();
    m_paused = false;
    m_spellbookOpen = false;
    m_inventoryOpen = false;
    m_runeConfigOpen = false;
    m_shadyShopOpen = false;
    m_consciousnessOpen = false;
    m_skillTreeOpen = false;
    m_artifactStorageOpen = false;
    m_runPromptOpen = false;
    m_runPromptDismissed = false;
    m_roomClearRewarded = false;
    saveProgress();
    updateCamera();
}

void Game::handleGameEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN: {
            const SDL_Scancode sc = event.key.keysym.scancode;
            // Esc opens the pause menu; quitting now happens through the menu's
            // confirmed Quit Game button. Guard on repeat == 0 so holding Esc
            // opens it once instead of toggling every repeat.
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                if (event.key.repeat == 0) {
                    m_paused = true;
                    m_pauseScreen = PauseScreen::Main;
                    m_rebindingAction = -1;
                }
            } else if (sc == m_keybinds.get(Action::Spellbook)) {
                // Hold-to-view overlay: opens here, closes on the key's release.
                m_spellbookOpen = true;
            } else if (sc == m_keybinds.get(Action::Inventory) && event.key.repeat == 0) {
                m_inventoryOpen = true;
                m_draggedInventoryItem = DraggedInventoryItem{};
            } else if (event.key.repeat == 0 && m_player) {
                // One-shot keyboard actions (roll/use); movement is polled.
                if (sc == m_keybinds.get(Action::Roll)) {
                    m_player->roll();
                } else if (sc == m_keybinds.get(Action::UseItem)) {
                    if (m_rooms.isHub() &&
                        m_rooms.playerInRuneVendor(m_player->x(), m_player->y(),
                                                   m_player->size())) {
                        m_runeConfigOpen = true;
                        m_selectedRuneLoadoutSlot = 0;
                        m_draggedRune = DraggedRune{};
                    } else if (m_rooms.isHub() &&
                               m_rooms.playerInShadyVendor(m_player->x(), m_player->y(),
                                                           m_player->size())) {
                        m_shadyShopOpen = true;
                    } else if (m_rooms.isHub() &&
                               m_rooms.playerInConsciousnessConsole(m_player->x(),
                                                                    m_player->y(),
                                                                    m_player->size())) {
                        m_consciousnessOpen = true;
                    } else if (m_rooms.isHub() &&
                               m_rooms.playerInSkillTreeConsole(m_player->x(),
                                                                m_player->y(),
                                                                m_player->size())) {
                        m_skillTreeOpen = true;
                    } else if (m_rooms.isHub() &&
                               m_rooms.playerInArtifactStorage(m_player->x(),
                                                               m_player->y(),
                                                               m_player->size())) {
                        m_artifactStorageOpen = true;
                        m_artifactScrollOffset =
                            clampInt(m_selectedArtifactIndex - (m_selectedArtifactIndex % kArtifactColumns),
                                     0,
                                     static_cast<int>(m_artifactOptions.size()) > kArtifactVisibleCount
                                         ? static_cast<int>(m_artifactOptions.size()) - kArtifactVisibleCount
                                         : 0);
                    } else {
                        m_player->useItem();
                    }
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            // The run prompt is modal to the mouse: while it's up, a left click
            // only hits its Yes/No buttons and never falls through to an attack.
            if (m_runPromptOpen) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    const SDL_Point lp =
                        windowToLogical(event.button.x, event.button.y);
                    const SDL_Rect yes = runPromptYesButtonRect();
                    const SDL_Rect no  = runPromptNoButtonRect();
                    if (SDL_PointInRect(&lp, &yes)) {
                        acceptRunPrompt();
                    } else if (SDL_PointInRect(&lp, &no)) {
                        m_runPromptOpen = false;
                        m_runPromptDismissed = true;
                    }
                }
                break;
            }
            // Left mouse button swings the melee attack.
            if (event.button.button == SDL_BUTTON_LEFT && m_player) {
                if (m_player->isCasting()) {
                    m_player->appendCastInput(CastInput::Left);
                } else {
                    // Swing toward the cursor: map the click into world space.
                    const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                    m_player->attack(static_cast<float>(lp.x) + m_cameraX,
                                     static_cast<float>(lp.y) + m_cameraY);
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

void Game::handleInventoryEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN: {
            const SDL_Scancode sc = event.key.keysym.scancode;
            if ((event.key.keysym.sym == SDLK_ESCAPE ||
                 sc == m_keybinds.get(Action::Inventory)) &&
                event.key.repeat == 0) {
                m_inventoryOpen = false;
                m_draggedInventoryItem = DraggedInventoryItem{};
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                bool equipment = false;
                const int slot = inventorySlotAt(lp.x, lp.y, &equipment);
                if (slot >= 0) {
                    const int item = equipment ? m_equipmentSlots[slot]
                                               : m_inventorySlots[slot];
                    if (item >= 0) {
                        m_draggedInventoryItem.active = true;
                        m_draggedInventoryItem.fromEquipment = equipment;
                        m_draggedInventoryItem.fromSlot = slot;
                        m_draggedInventoryItem.itemIndex = item;
                    }
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT &&
                m_draggedInventoryItem.active) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                bool equipment = false;
                const int slot = inventorySlotAt(lp.x, lp.y, &equipment);
                if (slot >= 0) {
                    moveInventoryItem(m_draggedInventoryItem.fromEquipment,
                                      m_draggedInventoryItem.fromSlot,
                                      equipment, slot);
                }
                m_draggedInventoryItem = DraggedInventoryItem{};
            }
            break;
        default:
            break;
    }
}

void Game::handleRuneConfigEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.repeat == 0 &&
                (event.key.keysym.sym == SDLK_ESCAPE ||
                 event.key.keysym.scancode == m_keybinds.get(Action::UseItem))) {
                m_runeConfigOpen = false;
                m_runeResearchOpen = false;
                m_draggedRune = DraggedRune{};
                m_draggedResearchRune = DraggedResearchRune{};
            } else if (!m_runeResearchOpen && event.key.keysym.sym == SDLK_UP) {
                m_runeLibraryScrollOffset = clampInt(m_runeLibraryScrollOffset - 1, 0,
                    static_cast<int>(allSpellRegistry().size()) > kRuneLibraryVisibleCount
                        ? static_cast<int>(allSpellRegistry().size()) - kRuneLibraryVisibleCount
                        : 0);
            } else if (!m_runeResearchOpen && event.key.keysym.sym == SDLK_DOWN) {
                m_runeLibraryScrollOffset = clampInt(m_runeLibraryScrollOffset + 1, 0,
                    static_cast<int>(allSpellRegistry().size()) > kRuneLibraryVisibleCount
                        ? static_cast<int>(allSpellRegistry().size()) - kRuneLibraryVisibleCount
                        : 0);
            }

            break;
        case SDL_MOUSEWHEEL:
            if (!m_runeResearchOpen && event.wheel.y != 0) {
                m_runeLibraryScrollOffset = clampInt(m_runeLibraryScrollOffset - event.wheel.y, 0,
                    static_cast<int>(allSpellRegistry().size()) > kRuneLibraryVisibleCount
                        ? static_cast<int>(allSpellRegistry().size()) - kRuneLibraryVisibleCount
                        : 0);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                auto contains = [&](const SDL_Rect& r) {
                    return lp.x >= r.x && lp.x < r.x + r.w &&
                           lp.y >= r.y && lp.y < r.y + r.h;
                };
                if (contains(runeResearchButtonRect())) {
                    m_runeResearchOpen = !m_runeResearchOpen;
                    m_draggedRune = DraggedRune{};
                    m_draggedResearchRune = DraggedResearchRune{};
                    return;
                }
                if (m_runeResearchOpen) {
                    const int runeItem = runeResearchInventoryRuneAt(lp.x, lp.y);
                    if (runeItem >= 0 &&
                        !m_inventoryItems[static_cast<std::size_t>(runeItem)].extracted) {
                        m_draggedResearchRune.active = true;
                        m_draggedResearchRune.itemIndex = runeItem;
                    }
                    return;
                }
                const std::vector<int>& loadout = spellLoadoutIndices();
                const int activeSlot = runeConfigActiveSlotAt(lp.x, lp.y);
                if (activeSlot >= 0 &&
                    activeSlot < static_cast<int>(loadout.size())) {
                    m_selectedRuneLoadoutSlot = activeSlot;
                    m_draggedRune.active = true;
                    m_draggedRune.fromLoadout = true;
                    m_draggedRune.fromSlot = activeSlot;
                    m_draggedRune.spellIndex = loadout[static_cast<std::size_t>(activeSlot)];
                    return;
                }
                const int librarySlot = runeConfigLibrarySlotAt(lp.x, lp.y);
                if (librarySlot >= 0) {
                    m_draggedRune.active = true;
                    m_draggedRune.fromLoadout = false;
                    m_draggedRune.fromSlot = librarySlot;
                    m_draggedRune.spellIndex = librarySlot;
                    return;
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT && m_draggedResearchRune.active) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                const SDL_Rect slot = runeResearchExtractionSlotRect();
                if (lp.x >= slot.x && lp.x < slot.x + slot.w &&
                    lp.y >= slot.y && lp.y < slot.y + slot.h) {
                    extractRuneItem(m_draggedResearchRune.itemIndex);
                }
                m_draggedResearchRune = DraggedResearchRune{};
            } else if (event.button.button == SDL_BUTTON_LEFT && m_draggedRune.active) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                const int targetSlot = runeConfigActiveSlotAt(lp.x, lp.y);
                if (targetSlot >= 0) {
                    if (setActiveSpellIndex(targetSlot, m_draggedRune.spellIndex)) {
                        saveProgress();
                    }
                    m_selectedRuneLoadoutSlot = targetSlot;
                }
                m_draggedRune = DraggedRune{};
            }
            break;
        default:
            break;
    }
}

void Game::handleShadyShopEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.repeat == 0 &&
                (event.key.keysym.sym == SDLK_ESCAPE ||
                 event.key.keysym.scancode == m_keybinds.get(Action::UseItem))) {
                m_shadyShopOpen = false;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT && !m_shadyPurchaseMade) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                auto contains = [&](const SDL_Rect& r) {
                    return lp.x >= r.x && lp.x < r.x + r.w &&
                           lp.y >= r.y && lp.y < r.y + r.h;
                };
                for (int i = 0; i < static_cast<int>(m_shadyShopItems.size()); ++i) {
                    if (contains(shadyShopItemRect(i))) {
                        if (m_shadyCurrency > 0 &&
                            addInventoryItem(m_shadyShopItems[static_cast<std::size_t>(i)])) {
                            --m_shadyCurrency;
                            m_shadyPurchaseMade = true;
                            saveProgress();
                        }
                        return;
                    }
                }
            }
            break;
        default:
            break;
    }
}

void Game::handleConsciousnessEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.repeat == 0 &&
                (event.key.keysym.sym == SDLK_ESCAPE ||
                 event.key.keysym.scancode == m_keybinds.get(Action::UseItem))) {
                m_consciousnessOpen = false;
            } else if (event.key.repeat == 0 &&
                       (event.key.keysym.sym == SDLK_LEFT ||
                        event.key.keysym.sym == SDLK_a)) {
                rotateCharacterSelection(-1);
            } else if (event.key.repeat == 0 &&
                       (event.key.keysym.sym == SDLK_RIGHT ||
                        event.key.keysym.sym == SDLK_d)) {
                rotateCharacterSelection(1);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                auto contains = [&](const SDL_Rect& r) {
                    return lp.x >= r.x && lp.x < r.x + r.w &&
                           lp.y >= r.y && lp.y < r.y + r.h;
                };
                if (contains(consciousnessLeftButtonRect()) ||
                    contains(consciousnessCardRect(-1))) {
                    rotateCharacterSelection(-1);
                } else if (contains(consciousnessRightButtonRect()) ||
                           contains(consciousnessCardRect(1))) {
                    rotateCharacterSelection(1);
                }
            }
            break;
        default:
            break;
    }
}

void Game::handleSkillTreeEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN &&
        event.key.repeat == 0 &&
        (event.key.keysym.sym == SDLK_ESCAPE ||
         event.key.keysym.scancode == m_keybinds.get(Action::UseItem))) {
        m_skillTreeOpen = false;
    }
}

void Game::handleArtifactStorageEvent(const SDL_Event& event) {
    const auto maxScrollOffset = [&]() {
        const int count = static_cast<int>(m_artifactOptions.size());
        return count > kArtifactVisibleCount ? count - kArtifactVisibleCount : 0;
    };
    const auto scrollBy = [&](int delta) {
        m_artifactScrollOffset =
            clampInt(m_artifactScrollOffset + delta, 0, maxScrollOffset());
    };

    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.repeat == 0 &&
                (event.key.keysym.sym == SDLK_ESCAPE ||
                 event.key.keysym.scancode == m_keybinds.get(Action::UseItem))) {
                m_artifactStorageOpen = false;
            } else if (event.key.keysym.sym == SDLK_UP) {
                scrollBy(-kArtifactColumns);
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                scrollBy(kArtifactColumns);
            } else if (event.key.keysym.sym == SDLK_PAGEUP) {
                scrollBy(-kArtifactVisibleCount);
            } else if (event.key.keysym.sym == SDLK_PAGEDOWN) {
                scrollBy(kArtifactVisibleCount);
            } else if (event.key.keysym.sym == SDLK_HOME) {
                m_artifactScrollOffset = 0;
            } else if (event.key.keysym.sym == SDLK_END) {
                m_artifactScrollOffset = maxScrollOffset();
            }
            break;
        case SDL_MOUSEWHEEL:
            if (event.wheel.y != 0) {
                scrollBy(-event.wheel.y * kArtifactColumns);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const SDL_Point lp = windowToLogical(event.button.x, event.button.y);
                auto contains = [&](const SDL_Rect& r) {
                   return lp.x >= r.x && lp.x < r.x + r.w &&
                          lp.y >= r.y && lp.y < r.y + r.h;
                };
                for (int slot = 0; slot < kArtifactVisibleCount; ++slot) {
                   const int artifactIndex = m_artifactScrollOffset + slot;
                   if (artifactIndex >= static_cast<int>(m_artifactOptions.size())) {
                       break;
                   }
                   if (contains(artifactCardRect(slot))) {
                       selectArtifact(artifactIndex);
                       return;
                   }
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
            if (event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0) {
                // Back out one level: close an open dropdown first, then a
                // sub-screen returns to Main, and Esc on Main resumes the game.
                // repeat == 0 keeps a held Esc from spazzing the menu open/shut.
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
        const bool inRun = !m_rooms.isHub();
        const int kCount = inRun ? 4 : 3;  // Resume, Exit Run, Settings, Quit Game
        if (hit(0, kCount)) {
            m_paused = false;
        } else if (inRun && hit(1, kCount)) {
            exitRunToHub();
        } else if (hit(inRun ? 2 : 1, kCount)) {
            m_pauseScreen = PauseScreen::Settings;
        } else if (hit(inRun ? 3 : 2, kCount)) {
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
    // The spellbook is a hold-to-view overlay: it stays up only while the
    // Spellbook key is held, so releasing that key closes it. Esc also closes
    // it as a fallback. Held-key repeats are ignored so it doesn't flicker.
    if (event.type == SDL_KEYUP &&
        event.key.keysym.scancode == m_keybinds.get(Action::Spellbook)) {
        m_spellbookOpen = false;
    } else if (event.type == SDL_KEYDOWN &&
               event.key.keysym.sym == SDLK_ESCAPE) {
        m_spellbookOpen = false;
    }
}

void Game::update(float deltaSeconds) {
    if (m_player) {
        if (m_playerDamageCooldown > 0.0f) {
            m_playerDamageCooldown -= deltaSeconds;
        }
        if (m_playerDisplayedHealth > static_cast<float>(m_playerHealth)) {
            m_playerDisplayedHealth -= 42.0f * deltaSeconds;
            if (m_playerDisplayedHealth < static_cast<float>(m_playerHealth)) {
                m_playerDisplayedHealth = static_cast<float>(m_playerHealth);
            }
        } else if (m_playerDisplayedHealth < static_cast<float>(m_playerHealth)) {
            m_playerDisplayedHealth = static_cast<float>(m_playerHealth);
        }
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        m_player->update(deltaSeconds, keys, m_keybinds);

        // Resolve wall collisions and door transitions against the active room.
        // Doors stay locked (acting as solid wall) until the room is cleared.
        float px = m_player->x();
        float py = m_player->y();
        const bool doorsUnlocked = m_rooms.isHub() || m_enemies.empty();
        const bool roomChanged =
            m_rooms.resolvePlayer(px, py, m_player->size(), doorsUnlocked);
        if (roomChanged) {
            m_playerSafeX = px;
            m_playerSafeY = py;
        } else if (!m_rooms.isHub() &&
                   m_rooms.playerOverPit(px, py, m_player->size())) {
            px = m_playerSafeX;
            py = m_playerSafeY;
        } else {
            m_playerSafeX = px;
            m_playerSafeY = py;
        }
        m_player->setPosition(px, py); 
        
        //resolve spell collisions with walls
        float sx = m_player->getSpellX();
        float sy = m_player->getSpellY();
        bool clearSpell = m_rooms.resolveSpell(sx, sy, m_player->spellSize());
        m_player->setSpellX(sx);
        m_player->setSpellY(sy);

        if (clearSpell) m_player->clearActiveSpell(SpellImpactKind::Wall);
       
        // Taking a door into a freshly generated run room loads its encounter.
        if (roomChanged && !m_rooms.isHub()) {
            m_enemies.spawnAt(m_rooms.currentEnemySpawns());
            m_roomClearRewarded = false;
        }
        const float playerCenterX = px + m_player->size() * 0.5f;
        const float playerCenterY = py + m_player->size() * 0.5f;
        if (!m_rooms.isHub()) {
            m_enemies.update(deltaSeconds, m_rooms.interiorRect(), m_rooms.current().holes,
                             playerCenterX, playerCenterY);
            if (m_playerDamageCooldown <= 0.0f && !m_player->isInvulnerable()) {
                const int damage = m_enemies.damagePlayer(px, py, m_player->size());
                if (damage > 0) {
                    setPlayerHealth(m_playerHealth - damage);
                    m_playerDamageCooldown = 0.75f;
                    if (m_playerHealth <= 0) {
                        handlePlayerDeath();
                        return;
                    }
                }
            }

            SDL_Rect hitbox{};
            if (m_player->attackHitbox(&hitbox)) {
                awardEnemyCurrencies(m_enemies.damageInRect(hitbox, 1));
            }

            float spellX = 0.0f;
            float spellY = 0.0f;
            float spellRadius = 0.0f;

            float spellDirectionX = 0.0f;
            float spellDirectionY = 0.0f;
            float knockbackPower = 0.0f;
            int spellDamage = 0;
            bool spellClearOnHit = true;

            if (m_player->activeSpellCircle(&spellX, &spellY, &spellRadius,
                                            &spellDirectionX, &spellDirectionY,
                                            &knockbackPower, &spellDamage,
                                            &spellClearOnHit)) {
                bool spellHit = false;
                const int killed = m_enemies.damageCircle(
                    spellX, spellY, spellRadius, spellDamage,
                    spellDirectionX, spellDirectionY, knockbackPower, &spellHit);
                awardEnemyCurrencies(killed);
                if (spellHit && spellClearOnHit) {
                    m_player->clearActiveSpell(SpellImpactKind::Enemy);
                }
            }
            if (m_enemies.empty() && !m_roomClearRewarded && !m_rooms.isVendorRoom()) {
                ++m_skillCurrency;
                m_roomClearRewarded = true;
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
    SDL_Texture* runeVendorTexture =
        m_textures ? m_textures->defaultFor(AssetKind::RuneVendor) : nullptr;
    SDL_Texture* shadyVendorTexture =
        m_textures ? m_textures->defaultFor(AssetKind::ShadyVendor) : nullptr;
    SDL_Texture* consciousnessConsoleTexture =
        m_textures ? m_textures->defaultFor(AssetKind::ConsciousnessConsole) : nullptr;
    SDL_Texture* skillTreeConsoleTexture =
        m_textures ? m_textures->defaultFor(AssetKind::SkillTreeConsole) : nullptr;
    SDL_Texture* artifactStorageTexture =
        m_textures ? m_textures->defaultFor(AssetKind::ArtifactStorage) : nullptr;
    SDL_Texture* runPortalTexture =
        m_textures ? m_textures->defaultFor(AssetKind::RunPortal) : nullptr;
    SDL_Texture* floorTexture =
        m_textures ? m_textures->defaultFor(AssetKind::Floor) : nullptr;
    HubTextures hubTextures;
    if (m_textures) {
        hubTextures.floor   = m_textures->defaultFor(AssetKind::HubFloor);
        hubTextures.wall    = m_textures->defaultFor(AssetKind::HubWall);
        hubTextures.lantern = m_textures->defaultFor(AssetKind::HubLantern);
        hubTextures.pillar  = m_textures->defaultFor(AssetKind::HubPillar);
        hubTextures.urn     = m_textures->defaultFor(AssetKind::HubUrn);
        hubTextures.banner  = m_textures->defaultFor(AssetKind::HubBanner);
    }
    m_rooms.render(m_renderer, m_cameraX, m_cameraY, vendorTexture,
                   runeVendorTexture, shadyVendorTexture,
                   consciousnessConsoleTexture, skillTreeConsoleTexture,
                   artifactStorageTexture, runPortalTexture, hubTextures,
                   floorTexture);
    if (!m_rooms.isHub()) {
        SDL_Texture* enemyTexture =
            m_textures ? m_textures->defaultFor(AssetKind::Enemy) : nullptr;
        m_enemies.render(m_renderer, m_cameraX, m_cameraY, enemyTexture);
    }

    // The player draws itself (sprite, melee hitbox, item effect).
    if (m_player) {
        m_player->render(m_renderer, m_cameraX, m_cameraY);
        // Re-draw any prop the player is standing behind, over the player, so
        // tall art (pillars, vendors, ...) occludes while its base still blocks.
        m_rooms.renderOccluders(m_renderer, m_cameraX, m_cameraY, vendorTexture,
                                runeVendorTexture, shadyVendorTexture,
                                consciousnessConsoleTexture, skillTreeConsoleTexture,
                                artifactStorageTexture, hubTextures,
                                m_player->y() + m_player->size());
    }

    // Vendor wares: name + price drawn above/below each pedestal (world space).
    // Buying is not wired yet; this just shows what is for sale.
    if (!m_rooms.isHub() && m_rooms.isVendorRoom()) {
        for (const ShopItem& item : m_rooms.currentShopItems()) {
            const int sx = item.rect.x + item.rect.w / 2 - static_cast<int>(m_cameraX);
            const int nameY = item.rect.y - 14 - static_cast<int>(m_cameraY);
            const int priceY = item.rect.y + item.rect.h + 14 - static_cast<int>(m_cameraY);
            drawTextCentered(item.name, sx, nameY, SDL_Color{235, 230, 210, 255});
            const std::string price = std::to_string(item.price) + "g";
            drawTextCentered(price.c_str(), sx, priceY, SDL_Color{245, 215, 120, 255});
        }
    }

    // How many rooms deep this run is (the door leads to the next one), plus the
    // room's status: enemies remaining / cleared, or a shop banner.
    if (!m_rooms.isHub()) {
        const std::string roomLabel =
            "Room " + std::to_string(m_rooms.runDepth() + 1);
        drawTextCentered(roomLabel.c_str(), m_width / 2, 24,
                         SDL_Color{200, 210, 230, 255});

        if (m_rooms.isVendorRoom()) {
            drawTextCentered("Shop", m_width / 2, 48, SDL_Color{245, 215, 120, 255});
        } else {
            const int remaining = m_enemies.count();
            if (remaining > 0) {
                const std::string enemyLabel =
                    "Enemies: " + std::to_string(remaining);
                drawTextCentered(enemyLabel.c_str(), m_width / 2, 48,
                                 SDL_Color{235, 120, 110, 255});
            } else {
                drawTextCentered("Room Cleared - door unlocked", m_width / 2, 48,
                                 SDL_Color{130, 235, 140, 255});
            }
        }
    }

    renderRunPrompt();

    if (m_player && m_rooms.isHub() &&
        m_rooms.playerInRuneVendor(m_player->x(), m_player->y(), m_player->size())) {
        drawTextCentered("Press E to configure spellbook runes",
                         m_width / 2, m_height - 110,
                         SDL_Color{150, 240, 255, 255});
    } else if (m_player && m_rooms.isHub() &&
               m_rooms.playerInShadyVendor(m_player->x(), m_player->y(),
                                           m_player->size())) {
        drawTextCentered(m_shadyPurchaseMade
                             ? "The shady dealer has nothing else for you."
                             : "Press E to browse risky run items",
                         m_width / 2, m_height - 110,
                         SDL_Color{235, 200, 120, 255});
    } else if (m_player && m_rooms.isHub() &&
               m_rooms.playerInConsciousnessConsole(m_player->x(), m_player->y(),
                                                    m_player->size())) {
        drawTextCentered("Press E to transfer consciousness",
                         m_width / 2, m_height - 110,
                         SDL_Color{190, 170, 255, 255});
    } else if (m_player && m_rooms.isHub() &&
               m_rooms.playerInSkillTreeConsole(m_player->x(), m_player->y(),
                                                m_player->size())) {
        drawTextCentered("Press E to view skill tree selector",
                         m_width / 2, m_height - 110,
                         SDL_Color{110, 255, 165, 255});
    } else if (m_player && m_rooms.isHub() &&
               m_rooms.playerInArtifactStorage(m_player->x(), m_player->y(),
                                               m_player->size())) {
        drawTextCentered("Press E to select a run artifact",
                         m_width / 2, m_height - 110,
                         SDL_Color{255, 190, 95, 255});
    }

    if (m_spellbookOpen) {
        renderSpellbook();
    }
    
    drawCursor();
    presentFrame();
}

void Game::shutdown() {
    saveProgress();
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

SDL_Rect Game::runPromptPanelRect() const {
    return SDL_Rect{m_width / 2 - 210, m_height / 2 - 80, 420, 160};
}

SDL_Rect Game::runPromptYesButtonRect() const {
    const SDL_Rect panel = runPromptPanelRect();
    return SDL_Rect{panel.x + 40, panel.y + 92, 150, 48};
}

SDL_Rect Game::runPromptNoButtonRect() const {
    const SDL_Rect panel = runPromptPanelRect();
    return SDL_Rect{panel.x + panel.w - 40 - 150, panel.y + 92, 150, 48};
}

void Game::renderRunPrompt() {
    if (!m_runPromptOpen) {
        return;
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    const SDL_Rect panel = runPromptPanelRect();
    SDL_SetRenderDrawColor(m_renderer, 10, 12, 18, 230);
    SDL_RenderFillRect(m_renderer, &panel);
    SDL_SetRenderDrawColor(m_renderer, 120, 210, 255, 255);
    SDL_RenderDrawRect(m_renderer, &panel);

    drawTextCentered("Start a run?",
                     panel.x + panel.w / 2, panel.y + 44,
                     SDL_Color{255, 255, 255, 255});

    // Clickable Yes / No buttons; highlight the one under the cursor.
    const SDL_Point m = mouseLogical();
    auto drawButton = [&](const SDL_Rect& r, const char* label,
                          SDL_Color base, SDL_Color hover, SDL_Color border) {
        const bool hot = SDL_PointInRect(&m, &r) == SDL_TRUE;
        const SDL_Color fill = hot ? hover : base;
        SDL_SetRenderDrawColor(m_renderer, fill.r, fill.g, fill.b, 255);
        SDL_RenderFillRect(m_renderer, &r);
        SDL_SetRenderDrawColor(m_renderer, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(m_renderer, &r);
        drawTextCentered(label, r.x + r.w / 2, r.y + r.h / 2,
                         SDL_Color{255, 255, 255, 255});
    };
    drawButton(runPromptYesButtonRect(), "Yes",
               SDL_Color{34, 92, 54, 255}, SDL_Color{56, 140, 84, 255},
               SDL_Color{120, 235, 150, 255});
    drawButton(runPromptNoButtonRect(), "No",
               SDL_Color{96, 40, 44, 255}, SDL_Color{150, 60, 66, 255},
               SDL_Color{245, 130, 130, 255});
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

SDL_Rect Game::drawTextWrappedCentered(const char* text, int cx, int cy,
                                       int wrapWidth, SDL_Color color) {
    SDL_Rect dst = {cx, cy, 0, 0};
    if (!m_font || !text || !*text || wrapWidth <= 0) {
        return dst;
    }
    SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(
        m_font, text, color, static_cast<Uint32>(wrapWidth));
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
    const bool liveGameplay =
        !m_mainMenuOpen && !m_paused && !m_spellbookOpen && !m_inventoryOpen &&
        !m_runeConfigOpen && !m_shadyShopOpen && !m_consciousnessOpen &&
        !m_skillTreeOpen && !m_artifactStorageOpen;
    if (liveGameplay) {
        drawCurrencyHud();
        drawSpellHotbar();
    }
    drawFpsOverlay();
    SDL_SetRenderTarget(m_renderer, nullptr);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    const SDL_Rect dst = canvasRect();
    SDL_RenderCopy(m_renderer, m_canvas, nullptr, &dst);
    SDL_RenderPresent(m_renderer);
}

void Game::drawCurrencyHud() {
    if (!m_renderer || !m_font || !m_textures) {
        return;
    }

    struct CurrencyRow {
        AssetKind kind;
        int amount;
        const char* label;
        SDL_Color textColor;
    };
    const CurrencyRow rows[] = {
        {AssetKind::GoldCurrency, m_gold, "Gold", SDL_Color{255, 220, 95, 255}},
        {AssetKind::SkillCurrency, m_skillCurrency, "Skill", SDL_Color{110, 255, 165, 255}},
        {AssetKind::ShadyCurrency, m_shadyCurrency, "Shady", SDL_Color{205, 150, 255, 255}},
    };

    constexpr int kIcon = 28;
    constexpr int kRowH = 32;
    const SDL_Rect panel{14, 14, 178, 112};
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 8, 10, 16, 190);
    SDL_RenderFillRect(m_renderer, &panel);
    SDL_SetRenderDrawColor(m_renderer, 70, 76, 92, 210);
    SDL_RenderDrawRect(m_renderer, &panel);

    for (int i = 0; i < 3; ++i) {
        const int y = panel.y + 10 + i * kRowH;
        const SDL_Rect icon{panel.x + 10, y + 1, kIcon, kIcon};
        if (SDL_Texture* tex = m_textures->defaultFor(rows[i].kind)) {
            SDL_RenderCopy(m_renderer, tex, nullptr, &icon);
        } else {
            SDL_SetRenderDrawColor(m_renderer, rows[i].textColor.r,
                                   rows[i].textColor.g, rows[i].textColor.b, 255);
            SDL_RenderFillRect(m_renderer, &icon);
        }
        const std::string text =
            std::string(rows[i].label) + ": " + std::to_string(rows[i].amount);
        drawTextCentered(text.c_str(), panel.x + 100, y + kIcon / 2,
                         rows[i].textColor);
    }

    const SDL_Rect healthPanel{14, m_height - 58, 300, 38};
    const SDL_Rect barBack{healthPanel.x + 12, healthPanel.y + 10,
                           healthPanel.w - 24, 18};
    const float actualPct = m_playerMaxHealth > 0
        ? static_cast<float>(m_playerHealth) / static_cast<float>(m_playerMaxHealth)
        : 0.0f;
    const float chipPct = m_playerMaxHealth > 0
        ? m_playerDisplayedHealth / static_cast<float>(m_playerMaxHealth)
        : 0.0f;
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 8, 10, 16, 190);
    SDL_RenderFillRect(m_renderer, &healthPanel);
    SDL_SetRenderDrawColor(m_renderer, 70, 76, 92, 210);
    SDL_RenderDrawRect(m_renderer, &healthPanel);
    SDL_SetRenderDrawColor(m_renderer, 36, 20, 24, 255);
    SDL_RenderFillRect(m_renderer, &barBack);

    SDL_Rect chipBar = barBack;
    chipBar.w = static_cast<int>(barBack.w * clamp01(chipPct));
    SDL_SetRenderDrawColor(m_renderer, 245, 190, 70, 220);
    SDL_RenderFillRect(m_renderer, &chipBar);

    SDL_Rect healthBar = barBack;
    healthBar.w = static_cast<int>(barBack.w * clamp01(actualPct));
    SDL_SetRenderDrawColor(m_renderer, 220, 55, 78, 255);
    SDL_RenderFillRect(m_renderer, &healthBar);
    SDL_SetRenderDrawColor(m_renderer, 255, 205, 210, 255);
    SDL_RenderDrawRect(m_renderer, &barBack);

    const std::string health =
        "HP: " + std::to_string(m_playerHealth) + "/" + std::to_string(m_playerMaxHealth);
    drawTextCentered(health.c_str(), healthPanel.x + healthPanel.w / 2,
                     healthPanel.y + healthPanel.h / 2,
                     SDL_Color{255, 135, 150, 255});
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

// Draw a small mouse glyph centred on (cx, cy): an outlined body with two top
// buttons, the active one filled with `accent`. Used in the spellbook to show
// left/right click inputs as icons instead of words. Returns the glyph width so
// callers can advance their layout cursor.
static int drawMouseIcon(SDL_Renderer* renderer, int cx, int cy,
                         bool highlightLeft, bool highlightRight,
                         SDL_Color accent) {
    constexpr int kW = 22;   // body width
    constexpr int kH = 32;   // body height
    constexpr int kButtonH = 12;  // height of the button band at the top
    const int left   = cx - kW / 2;
    const int top    = cy - kH / 2;
    const int midX   = cx;                 // divider between the two buttons
    const int bandY  = top + kButtonH;     // divider below the buttons

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Fill the highlighted button before the outline so the lines stay crisp.
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 200);
    if (highlightLeft) {
        SDL_Rect lhs{left + 1, top + 1, kW / 2 - 1, kButtonH - 1};
        SDL_RenderFillRect(renderer, &lhs);
    }
    if (highlightRight) {
        SDL_Rect rhs{midX + 1, top + 1, kW - kW / 2 - 2, kButtonH - 1};
        SDL_RenderFillRect(renderer, &rhs);
    }

    // Body outline plus the two internal dividers.
    SDL_SetRenderDrawColor(renderer, 200, 205, 215, 255);
    SDL_Rect body{left, top, kW, kH};
    SDL_RenderDrawRect(renderer, &body);
    SDL_RenderDrawLine(renderer, left, bandY, left + kW - 1, bandY);
    SDL_RenderDrawLine(renderer, midX, top, midX, bandY);

    return kW;
}

void Game::renderMainMenu() {
    if (m_pauseScreen == PauseScreen::Settings) {
        renderSettingsMenu();
        return;
    }

    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 6, 8, 16, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color title = {255, 245, 210, 255};
    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color hint  = {170, 175, 185, 255};

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < m_height; y += 8) {
        const Uint8 shade = static_cast<Uint8>(16 + y * 34 / m_height);
        SDL_SetRenderDrawColor(m_renderer, 8, 10, shade, 90);
        SDL_RenderDrawLine(m_renderer, 0, y, m_width, y);
    }
    for (int i = 0; i < 34; ++i) {
        const int x = (i * 137 + 41) % m_width;
        const int y = (i * 83 + 29) % (m_height - 130);
        const Uint8 alpha = static_cast<Uint8>(70 + (i % 4) * 35);
        SDL_SetRenderDrawColor(m_renderer, 180, 205, 255, alpha);
        SDL_RenderDrawPoint(m_renderer, x, y);
        if (i % 5 == 0) {
            SDL_RenderDrawPoint(m_renderer, x + 1, y);
        }
    }

    const int menuCenterX = 300;
    const int showcaseCenterX = m_width - 300;

    drawTextCentered("Gaia", menuCenterX, 96, title);
    drawTextCentered(m_mainMenuQuitConfirm
                         ? "Confirm before leaving Gaia."
                         : "Press Enter or click Start Game",
                     menuCenterX, 136, hint);

    const SDL_Rect showcase{showcaseCenterX - 190, 128, 380, 330};
    SDL_SetRenderDrawColor(m_renderer, 18, 22, 34, 210);
    SDL_RenderFillRect(m_renderer, &showcase);
    SDL_SetRenderDrawColor(m_renderer, 80, 92, 122, 220);
    SDL_RenderDrawRect(m_renderer, &showcase);
    SDL_SetRenderDrawColor(m_renderer, 45, 55, 88, 95);
    for (int y = showcase.y + 12; y < showcase.y + showcase.h - 12; y += 10) {
        SDL_RenderDrawLine(m_renderer, showcase.x + 14, y,
                          showcase.x + showcase.w - 14, y);
    }

    auto drawEllipse = [&](int cx, int cy, int rx, int ry, SDL_Color color) {
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        for (int y = -ry; y <= ry; ++y) {
            const float t = 1.0f - static_cast<float>(y * y) /
                static_cast<float>(ry * ry);
            const int half = static_cast<int>(rx * std::sqrt(t > 0.0f ? t : 0.0f));
            SDL_RenderDrawLine(m_renderer, cx - half, cy + y, cx + half, cy + y);
        }
    };
    drawEllipse(showcaseCenterX, 366, 118, 24, SDL_Color{72, 54, 36, 240});
    drawEllipse(showcaseCenterX, 356, 96, 17, SDL_Color{170, 126, 72, 230});
    SDL_Rect pedestal{showcaseCenterX - 106, 360, 212, 38};
    SDL_SetRenderDrawColor(m_renderer, 96, 70, 46, 255);
    SDL_RenderFillRect(m_renderer, &pedestal);
    SDL_SetRenderDrawColor(m_renderer, 235, 186, 98, 255);
    SDL_RenderDrawRect(m_renderer, &pedestal);

    SDL_Texture* characterTexture = m_textures
        ? m_textures->defaultFor(selectedCharacter().texture)
        : nullptr;
    if (characterTexture) {
        const SDL_Rect dst{showcaseCenterX - 104, 152, 208, 208};
        SDL_RenderCopy(m_renderer, characterTexture, nullptr, &dst);
    }

    const SDL_Point mlp = mouseLogical();
    const int mouseX = mlp.x, mouseY = mlp.y;
    auto hovered = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };
    auto mainMenuRow = [&](int index, int count) {
        constexpr int kRowW = 380;
        constexpr int kRowH = 46;
        constexpr int kSpacing = 14;
        const int totalH = count * kRowH + (count - 1) * kSpacing;
        return SDL_Rect{110, m_height / 2 - totalH / 2 + 84 + index * (kRowH + kSpacing),
                        kRowW, kRowH};
    };

    if (m_mainMenuQuitConfirm) {
        constexpr int kCount = 2;
        const char* labels[kCount] = {"Quit to Desktop", "Cancel"};
        drawTextCentered("Quit Game?", menuCenterX, m_height / 2 + 10, white);
        for (int i = 0; i < kCount; ++i) {
            const SDL_Rect row = mainMenuRow(i, kCount);
            drawRow(m_renderer, row, hovered(row));
            drawTextCentered(labels[i], row.x + row.w / 2, row.y + row.h / 2, white);
        }
    } else {
        constexpr int kCount = 3;
        const char* labels[kCount] = {"Start Game", "Settings", "Quit Game"};
        for (int i = 0; i < kCount; ++i) {
            const SDL_Rect row = mainMenuRow(i, kCount);
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

    const bool inRun = !m_rooms.isHub();
    const int kCount = inRun ? 4 : 3;
    const char* runLabels[] = {"Resume", "Exit Run", "Settings", "Quit Game"};
    const char* hubLabels[] = {"Resume", "Settings", "Quit Game"};
    for (int i = 0; i < kCount; ++i) {
        const SDL_Rect row = menuRowRect(i, kCount);
        drawRow(m_renderer, row, hovered(row));
        drawTextCentered(inRun ? runLabels[i] : hubLabels[i],
                         row.x + row.w / 2, row.y + row.h / 2, white);
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
    const SDL_Color ink   = {54, 38, 28, 255};
    const SDL_Color hint  = {92, 72, 54, 255};
    const SDL_Color combo = {112, 70, 160, 255};

    const SDL_Rect book{
        m_width / 2 - 500,
        m_height / 2 - 285,
        1000,
        570};
    const SDL_Rect leftPage{book.x + 28, book.y + 34, book.w / 2 - 44, book.h - 68};
    const SDL_Rect rightPage{book.x + book.w / 2 + 16, book.y + 34,
                             book.w / 2 - 44, book.h - 68};

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 35, 22, 14, 205);
    SDL_RenderFillRect(m_renderer, &book);
    SDL_SetRenderDrawColor(m_renderer, 220, 196, 145, 218);
    SDL_RenderFillRect(m_renderer, &leftPage);
    SDL_RenderFillRect(m_renderer, &rightPage);
    SDL_SetRenderDrawColor(m_renderer, 112, 78, 48, 235);
    SDL_RenderDrawRect(m_renderer, &book);
    SDL_RenderDrawRect(m_renderer, &leftPage);
    SDL_RenderDrawRect(m_renderer, &rightPage);
    const SDL_Rect spine{m_width / 2 - 10, book.y + 24, 20, book.h - 48};
    SDL_SetRenderDrawColor(m_renderer, 78, 48, 34, 190);
    SDL_RenderFillRect(m_renderer, &spine);

    drawTextCentered("Spellbook", m_width / 2, book.y - 26,
                     SDL_Color{245, 232, 190, 255});

    const std::vector<SpellDef>& spells = spellRegistry();
    const int rowCount = static_cast<int>(spells.size());
    for (int i = 0; i < rowCount; ++i) {
        const SpellDef& spell = spells[i];
        const SDL_Rect page = i % 2 == 0 ? leftPage : rightPage;
        const int localRow = i / 2;
        const SDL_Rect row{
            page.x + 24,
            page.y + 56 + localRow * 78,
            page.w - 48,
            56};
        SDL_SetRenderDrawColor(m_renderer, 96, 68, 45, 55);
        SDL_RenderFillRect(m_renderer, &row);
        SDL_SetRenderDrawColor(m_renderer, 96, 68, 45, 130);
        SDL_RenderDrawRect(m_renderer, &row);

        // Spell name on the left, its ordered input combo on the right.
        const int textY = row.y + row.h / 2;
        drawTextCentered(spell.name, row.x + 108, textY, ink);

        // Render the input combo as mouse-button icons separated by arrows,
        // centred where the old combo text used to sit.
        const int count = static_cast<int>(spell.sequence.size());
        constexpr int kIconW  = 22;
        constexpr int kArrowW = 40;  // space allotted to each "->" separator
        const int totalW = count * kIconW + (count > 0 ? count - 1 : 0) * kArrowW;
        const int centerX = row.x + row.w - 118;
        int cursorX = centerX - totalW / 2;
        for (int s = 0; s < count; ++s) {
            if (s != 0) {
                drawTextCentered("->", cursorX + kArrowW / 2, textY, combo);
                cursorX += kArrowW;
            }
            const bool isLeft = spell.sequence[s] == CastInput::Left;
            drawMouseIcon(m_renderer, cursorX + kIconW / 2, textY,
                          isLeft, !isLeft, combo);
            cursorX += kIconW;
        }
    }

}

// A neutral step dot for the hotbar progress row. It shows how many inputs a
// spell takes and how far the current chain has advanced, WITHOUT revealing
// which buttons are needed — the real L/R combos appear only in the spellbook.
static void drawStepDot(SDL_Renderer* renderer, int cx, int cy,
                        SDL_Color col, Uint8 alpha, bool filled) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const SDL_Rect dot{cx - 3, cy - 3, 6, 6};
    if (filled) {
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, alpha);
        SDL_RenderFillRect(renderer, &dot);
    }
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b,
                           static_cast<Uint8>(alpha > 140 ? alpha : 140));
    SDL_RenderDrawRect(renderer, &dot);
}

void Game::drawSpellHotbar() {
    if (!m_renderer || !m_player) {
        return;
    }
    const std::vector<SpellDef>& spells = spellRegistry();
    const int n = static_cast<int>(spells.size());
    if (n == 0) {
        return;
    }

    const bool casting = m_player->isCasting();
    const std::vector<CastInput>& seq = m_player->currentCastSequence();
    const int seqLen = static_cast<int>(seq.size());

    // How many leading inputs of the entered chain match this spell's sequence,
    // or -1 if the chain has diverged (this spell is no longer reachable).
    auto matchedPrefix = [&](const SpellDef& sp) -> int {
        if (seqLen > static_cast<int>(sp.sequence.size())) {
            return -1;
        }
        for (int i = 0; i < seqLen; ++i) {
            if (seq[i] != sp.sequence[i]) {
                return -1;
            }
        }
        return seqLen;
    };

    const float pulse = 0.5f + 0.5f *
        std::sin(static_cast<float>(SDL_GetTicks()) * 0.008f);

    constexpr int kBox = 54;
    constexpr int kGap = 8;
    const int totalW = n * kBox + (n - 1) * kGap;
    const int startX = m_width / 2 - totalW / 2;
    const int boxY = m_height - kBox - 16;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // While a cast is in progress, show the entered chain and a countdown of the
    // cast window above the bar, so the player can read their input and timing.
    if (casting) {
        const int cx = m_width / 2;
        const int barY = boxY - 12;
        int barW = totalW < 260 ? totalW : 260;
        const float remaining = m_player->castWindow() > 0.0f
            ? m_player->castTimeRemaining() / m_player->castWindow()
            : 0.0f;
        SDL_Rect timeBack{cx - barW / 2, barY, barW, 4};
        SDL_SetRenderDrawColor(m_renderer, 18, 22, 30, 200);
        SDL_RenderFillRect(m_renderer, &timeBack);
        SDL_Rect timeFill = timeBack;
        timeFill.w = static_cast<int>(barW * clamp01(remaining));
        SDL_SetRenderDrawColor(m_renderer, 120, 220, 255, 235);
        SDL_RenderFillRect(m_renderer, &timeFill);
        // The entered inputs are intentionally NOT shown here — the actual
        // L/R combos live only in the spellbook. The hotbar only conveys
        // progress and reachability (below).
    }

    for (int i = 0; i < n; ++i) {
        const SpellDef& sp = spells[i];
        const SDL_Rect box{startX + i * (kBox + kGap), boxY, kBox, kBox};
        const int count = static_cast<int>(sp.sequence.size());

        const int matched = matchedPrefix(sp);
        const bool available = !casting || matched >= 0;
        const bool exact = casting && matched >= 0 && seqLen == count;

        // Box background — dimmer when the spell can no longer be cast.
        SDL_SetRenderDrawColor(m_renderer,
                               available ? 22 : 11,
                               available ? 24 : 12,
                               available ? 33 : 15,
                               available ? 210 : 200);
        SDL_RenderFillRect(m_renderer, &box);

        // Spell icon: a gem in the spell's colour (greyed out when unavailable).
        const SDL_Color c = sp.color;
        const Uint8 iconA = available ? 255 : 55;
        const int iconCx = box.x + box.w / 2;
        const int iconCy = box.y + 20;
        constexpr int ir = 13;
        SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, iconA);
        for (int dy = -ir; dy <= ir; ++dy) {
            const int half = ir - std::abs(dy);
            SDL_RenderDrawLine(m_renderer, iconCx - half, iconCy + dy,
                               iconCx + half, iconCy + dy);
        }
        // Soft top highlight on the gem.
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255,
                               static_cast<Uint8>(available ? 95 : 25));
        for (int dy = -ir + 3; dy <= -2; ++dy) {
            const int half = (ir - 3) - std::abs(dy + ir - 3);
            if (half > 0) {
                SDL_RenderDrawLine(m_renderer, iconCx - half, iconCy + dy,
                                   iconCx + half, iconCy + dy);
            }
        }
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, available ? 150 : 110);
        SDL_Point gem[5] = {{iconCx, iconCy - ir}, {iconCx + ir, iconCy},
                            {iconCx, iconCy + ir}, {iconCx - ir, iconCy},
                            {iconCx, iconCy - ir}};
        SDL_RenderDrawLines(m_renderer, gem, 5);

        // Progress dots along the bottom: one per step in the spell's chain.
        // They reveal length and how far you've advanced, but not which inputs
        // (those stay secret to the spellbook). Entered steps fill in, the next
        // step pulses, the rest are hollow.
        constexpr int pipGap = 11;
        int px = iconCx - (count * pipGap) / 2 + pipGap / 2;
        const int pipY = box.y + box.h - 9;
        for (int s = 0; s < count; ++s) {
            SDL_Color pc;
            Uint8 a;
            bool filled;
            if (!available) {
                pc = SDL_Color{120, 122, 132, 255};
                a = 60;
                filled = false;
            } else if (casting && s < seqLen) {
                pc = SDL_Color{120, 235, 255, 255};   // step entered
                a = 255;
                filled = true;
            } else if (casting && s == seqLen && !exact) {
                pc = SDL_Color{255, 238, 170, 255};   // awaiting next input
                a = static_cast<Uint8>(150 + pulse * 105);
                filled = true;
            } else {
                pc = SDL_Color{198, 205, 220, 255};   // remaining / idle
                a = 150;
                filled = false;
            }
            drawStepDot(m_renderer, px, pipY, pc, a, filled);
            px += pipGap;
        }

        // Border / status overlay.
        if (exact) {
            // Ready: a pulsing green frame plus a confirming tick in the corner.
            const Uint8 g = static_cast<Uint8>(170 + pulse * 85);
            SDL_SetRenderDrawColor(m_renderer, 80, 230, 120, g);
            SDL_RenderDrawRect(m_renderer, &box);
            const SDL_Rect b2{box.x - 1, box.y - 1, box.w + 2, box.h + 2};
            SDL_RenderDrawRect(m_renderer, &b2);
            const SDL_Rect b3{box.x - 2, box.y - 2, box.w + 4, box.h + 4};
            SDL_SetRenderDrawColor(m_renderer, 80, 230, 120,
                                   static_cast<Uint8>(g / 2));
            SDL_RenderDrawRect(m_renderer, &b3);
            SDL_SetRenderDrawColor(m_renderer, 185, 255, 200, 255);
            SDL_RenderDrawLine(m_renderer, box.x + 6, box.y + 11,
                               box.x + 10, box.y + 15);
            SDL_RenderDrawLine(m_renderer, box.x + 10, box.y + 15,
                               box.x + 17, box.y + 6);
        } else if (casting && available) {
            // Still reachable with the current chain.
            SDL_SetRenderDrawColor(m_renderer, 90, 210, 255, 235);
            SDL_RenderDrawRect(m_renderer, &box);
            const SDL_Rect b2{box.x - 1, box.y - 1, box.w + 2, box.h + 2};
            SDL_SetRenderDrawColor(m_renderer, 90, 210, 255, 110);
            SDL_RenderDrawRect(m_renderer, &b2);
        } else if (casting && !available) {
            // No longer reachable: dark frame with a faint cross.
            SDL_SetRenderDrawColor(m_renderer, 58, 62, 72, 220);
            SDL_RenderDrawRect(m_renderer, &box);
            SDL_SetRenderDrawColor(m_renderer, 180, 70, 70, 80);
            SDL_RenderDrawLine(m_renderer, box.x + 7, box.y + 7,
                               box.x + box.w - 7, box.y + box.h - 7);
            SDL_RenderDrawLine(m_renderer, box.x + box.w - 7, box.y + 7,
                               box.x + 7, box.y + box.h - 7);
        } else {
            // Idle: neutral frame.
            SDL_SetRenderDrawColor(m_renderer, 82, 88, 104, 210);
            SDL_RenderDrawRect(m_renderer, &box);
        }
    }
}

const char* Game::equipmentSlotName(EquipmentSlot slot) {
    switch (slot) {
        case EquipmentSlot::Weapon:  return "Weapon";
        case EquipmentSlot::Armor:   return "Armor";
        case EquipmentSlot::Trinket: return "Trinket";
        case EquipmentSlot::Relic:   return "Relic";
        case EquipmentSlot::Artifact: return "Artifact";
        case EquipmentSlot::None:    return "Bag";
    }
    return "";
}

void Game::setPlayerHealth(int value) {
    const int previous = m_playerHealth;
    m_playerHealth = clampInt(value, 0, m_playerMaxHealth);
    if (m_playerHealth >= previous) {
        m_playerDisplayedHealth = static_cast<float>(m_playerHealth);
    }
}

void Game::captureRunSnapshot() {
    m_runSnapshot.valid = true;
    m_runSnapshot.skillCurrency = m_skillCurrency;
    m_runSnapshot.shadyCurrency = m_shadyCurrency;
    m_runSnapshot.selectedCharacterIndex = m_selectedCharacterIndex;
    m_runSnapshot.selectedArtifactIndex = m_selectedArtifactIndex;
    m_runSnapshot.shadyPurchaseMade = m_shadyPurchaseMade;
    m_runSnapshot.researchedSpells = m_researchedSpells;
    m_runSnapshot.artifactUnlocked.clear();
    for (const ArtifactOption& artifact : m_artifactOptions) {
        m_runSnapshot.artifactUnlocked.push_back(artifact.unlocked);
    }
    m_runSnapshot.runeExtracted.clear();
    for (const InventoryItem& item : m_inventoryItems) {
        if (item.kind == InventoryItemKind::Rune) {
            m_runSnapshot.runeExtracted.push_back(item.extracted);
        }
    }
    m_runSnapshot.loadout = spellLoadoutIndices();
    m_runSnapshot.inventorySlots.assign(m_inventorySlots.begin(), m_inventorySlots.end());
    m_runSnapshot.equipmentSlots.assign(m_equipmentSlots.begin(), m_equipmentSlots.end());
}

void Game::restoreRunSnapshot() {
    if (!m_runSnapshot.valid) {
        return;
    }
    m_skillCurrency = m_runSnapshot.skillCurrency;
    m_shadyCurrency = m_runSnapshot.shadyCurrency;
    m_selectedCharacterIndex = clampInt(
        m_runSnapshot.selectedCharacterIndex, 0,
        static_cast<int>(m_characterOptions.size()) - 1);
    if (m_player) {
        m_player->setCharacterKind(selectedCharacter().texture);
    }
    for (std::size_t i = 0; i < m_artifactOptions.size() &&
                            i < m_runSnapshot.artifactUnlocked.size(); ++i) {
        m_artifactOptions[i].unlocked = m_runSnapshot.artifactUnlocked[i];
    }
    int runeIndex = 0;
    for (InventoryItem& item : m_inventoryItems) {
        if (item.kind == InventoryItemKind::Rune &&
            runeIndex < static_cast<int>(m_runSnapshot.runeExtracted.size())) {
            item.extracted = m_runSnapshot.runeExtracted[static_cast<std::size_t>(runeIndex++)];
        }
    }
    if (m_runSnapshot.loadout.size() == spellLoadoutIndices().size()) {
        std::vector<int>& loadout =
            const_cast<std::vector<int>&>(spellLoadoutIndices());
        loadout = m_runSnapshot.loadout;
    }
    for (std::size_t i = 0; i < m_inventorySlots.size() &&
                            i < m_runSnapshot.inventorySlots.size(); ++i) {
        m_inventorySlots[i] = m_runSnapshot.inventorySlots[i];
    }
    for (std::size_t i = 0; i < m_equipmentSlots.size() &&
                            i < m_runSnapshot.equipmentSlots.size(); ++i) {
        m_equipmentSlots[i] = m_runSnapshot.equipmentSlots[i];
    }
    m_shadyPurchaseMade = m_runSnapshot.shadyPurchaseMade;
    selectArtifact(m_runSnapshot.selectedArtifactIndex);
}

void Game::removeNonRuneInventory() {
    for (int& slot : m_inventorySlots) {
        if (slot >= 0 &&
            slot < static_cast<int>(m_inventoryItems.size()) &&
            m_inventoryItems[static_cast<std::size_t>(slot)].kind != InventoryItemKind::Rune) {
            slot = -1;
        }
    }
    for (int i = 0; i < static_cast<int>(m_equipmentSlots.size()); ++i) {
        const int itemIndex = m_equipmentSlots[static_cast<std::size_t>(i)];
        if (i != static_cast<int>(EquipmentSlot::Artifact) &&
            itemIndex >= 0 &&
            itemIndex < static_cast<int>(m_inventoryItems.size()) &&
            m_inventoryItems[static_cast<std::size_t>(itemIndex)].kind != InventoryItemKind::Rune) {
            m_equipmentSlots[static_cast<std::size_t>(i)] = -1;
        }
    }
}

void Game::saveProgress() const {
    if (!m_progressReady) {
        return;
    }
    if (m_rooms.isHub() == false) {
        return;
    }
    const std::string path = progressSavePath();
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "Failed to save progress to %s\n", path.c_str());
        return;
    }
    std::vector<bool> artifactUnlocked;
    for (const ArtifactOption& artifact : m_artifactOptions) {
        artifactUnlocked.push_back(artifact.unlocked);
    }
    std::vector<bool> runeExtracted;
    for (const InventoryItem& item : m_inventoryItems) {
        if (item.kind == InventoryItemKind::Rune) {
            runeExtracted.push_back(item.extracted);
        }
    }
    std::vector<int> inventorySlots(m_inventorySlots.begin(), m_inventorySlots.end());
    std::vector<int> equipmentSlots(m_equipmentSlots.begin(), m_equipmentSlots.end());

    out << "version=1\n";
    out << "skill=" << m_skillCurrency << "\n";
    out << "shady=" << m_shadyCurrency << "\n";
    out << "character=" << m_selectedCharacterIndex << "\n";
    out << "artifact=" << m_selectedArtifactIndex << "\n";
    out << "shadyPurchase=" << (m_shadyPurchaseMade ? 1 : 0) << "\n";
    out << "researched=" << joinBools(m_researchedSpells) << "\n";
    out << "artifacts=" << joinBools(artifactUnlocked) << "\n";
    out << "runes=" << joinBools(runeExtracted) << "\n";
    out << "loadout=" << joinInts(spellLoadoutIndices()) << "\n";
    out << "inventorySlots=" << joinInts(inventorySlots) << "\n";
    out << "equipmentSlots=" << joinInts(equipmentSlots) << "\n";
}

void Game::ensureStarterDefaults() {
    for (std::size_t i = 0; i < m_researchedSpells.size(); ++i) {
        m_researchedSpells[i] = true;
    }

    auto hasItemInSlots = [&](int itemIndex) {
        for (int slotItem : m_inventorySlots) {
            if (slotItem == itemIndex) {
                return true;
            }
        }
        for (int slotItem : m_equipmentSlots) {
            if (slotItem == itemIndex) {
                return true;
            }
        }
        return false;
    };

    auto placeInBagIfMissing = [&](int itemIndex) {
        if (itemIndex < 0 ||
            itemIndex >= static_cast<int>(m_inventoryItems.size()) ||
            hasItemInSlots(itemIndex)) {
            return;
        }
        for (int& slot : m_inventorySlots) {
            if (slot < 0) {
                slot = itemIndex;
                return;
            }
        }
    };

    // Keep the prototype starter kit visible even when older saves predate
    // inventory persistence or saved an empty bag.
    for (int i = 0; i < 7; ++i) {
        placeInBagIfMissing(i);
    }
}

void Game::loadProgress() {
    m_progressReady = false;
    const std::string path = progressSavePath();
    std::ifstream in(path);
    if (!in && path != "gaia_save.txt") {
        in.open("gaia_save.txt");
    }
    if (!in) {
        ensureStarterDefaults();
        setPlayerHealth(m_playerMaxHealth);
        m_progressReady = true;
        saveProgress();
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "skill") {
            m_skillCurrency = std::atoi(value.c_str());
        } else if (key == "shady") {
            m_shadyCurrency = std::atoi(value.c_str());
        } else if (key == "character") {
            m_selectedCharacterIndex =
                clampInt(std::atoi(value.c_str()), 0,
                         static_cast<int>(m_characterOptions.size()) - 1);
        } else if (key == "artifact") {
            m_selectedArtifactIndex = std::atoi(value.c_str());
        } else if (key == "shadyPurchase") {
            m_shadyPurchaseMade = std::atoi(value.c_str()) != 0;
        } else if (key == "researched") {
            std::vector<bool> loaded = parseBoolList(value);
            for (std::size_t i = 0; i < loaded.size() && i < m_researchedSpells.size(); ++i) {
                m_researchedSpells[i] = loaded[i];
            }
        } else if (key == "artifacts") {
            std::vector<bool> loaded = parseBoolList(value);
            for (std::size_t i = 0; i < loaded.size() && i < m_artifactOptions.size(); ++i) {
                m_artifactOptions[i].unlocked = loaded[i];
            }
        } else if (key == "runes") {
            std::vector<bool> loaded = parseBoolList(value);
            int runeIndex = 0;
            for (InventoryItem& item : m_inventoryItems) {
                if (item.kind == InventoryItemKind::Rune &&
                    runeIndex < static_cast<int>(loaded.size())) {
                    item.extracted = loaded[static_cast<std::size_t>(runeIndex++)];
                }
            }
        } else if (key == "loadout") {
            std::vector<int> loaded = parseIntList(value);
            if (loaded.size() == spellLoadoutIndices().size()) {
                const int spellCount = static_cast<int>(allSpellRegistry().size());
                bool valid = true;
                for (int spellIndex : loaded) {
                    if (spellIndex < 0 || spellIndex >= spellCount) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    std::vector<int>& loadout =
                        const_cast<std::vector<int>&>(spellLoadoutIndices());
                    loadout = loaded;
                }
            }
        } else if (key == "inventorySlots") {
            std::vector<int> loaded = parseIntList(value);
            for (std::size_t i = 0; i < loaded.size() && i < m_inventorySlots.size(); ++i) {
                if (loaded[i] >= -1 && loaded[i] < static_cast<int>(m_inventoryItems.size())) {
                    m_inventorySlots[i] = loaded[i];
                }
            }
        } else if (key == "equipmentSlots") {
            std::vector<int> loaded = parseIntList(value);
            for (std::size_t i = 0; i < loaded.size() && i < m_equipmentSlots.size(); ++i) {
                if (loaded[i] >= -1 && loaded[i] < static_cast<int>(m_inventoryItems.size())) {
                    m_equipmentSlots[i] = loaded[i];
                }
            }
        }
    }
    if (m_player) {
        m_player->setCharacterKind(selectedCharacter().texture);
    }
    ensureStarterDefaults();
    if (!selectArtifact(m_selectedArtifactIndex)) {
        selectArtifact(0);
    }
    setPlayerHealth(m_playerMaxHealth);
    m_progressReady = true;
    saveProgress();
}

void Game::initializeInventory() {
    m_inventorySlots.fill(-1);
    m_equipmentSlots.fill(-1);
    m_draggedInventoryItem = DraggedInventoryItem{};
    m_inventoryItems = {
        {"Training Sword", "Placeholder weapon for testing equipment slots.",
         EquipmentSlot::Weapon, SDL_Color{220, 220, 235, 255}},
        {"Patchwork Vest", "Placeholder armor for testing drag/drop equipment.",
         EquipmentSlot::Armor, SDL_Color{120, 190, 230, 255}},
        {"Lucky Pebble", "Placeholder trinket for testing inventory movement.",
         EquipmentSlot::Trinket, SDL_Color{235, 205, 95, 255}},
        {"Test Potion", "Placeholder consumable. It stays in bag slots for now.",
         EquipmentSlot::None, SDL_Color{220, 90, 110, 255}},
        {"Cracked Ember Rune", "Research this rune at the engine to extract Thunder Hook, Meteor Seed, and Blood Moon.",
         EquipmentSlot::None, SDL_Color{255, 110, 55, 255}, InventoryItemKind::Rune,
         std::array<int, 3>{10, 11, 14}, 3, false},
        {"Drowned Mirror Rune", "Research this rune at the engine to extract Mirror Bolt and Prismatic Drill.",
         EquipmentSlot::None, SDL_Color{120, 205, 255, 255}, InventoryItemKind::Rune,
         std::array<int, 3>{15, 16, -1}, 2, false},
        {"Verdant Star Rune", "Research this rune at the engine to extract Gale Cutter and Rune Star.",
         EquipmentSlot::None, SDL_Color{120, 255, 150, 255}, InventoryItemKind::Rune,
         std::array<int, 3>{12, 13, -1}, 2, false},
    };
    for (ArtifactOption& artifact : m_artifactOptions) {
        m_inventoryItems.push_back(InventoryItem{
            artifact.name, artifact.description, EquipmentSlot::Artifact,
            artifact.color});
        artifact.itemIndex = static_cast<int>(m_inventoryItems.size()) - 1;
    }
    selectArtifact(0);
    m_shadyShopItems = {
        {"Glass Fang", "+High damage. Downside: breaks easily and leaves you fragile.",
         EquipmentSlot::Weapon, SDL_Color{190, 235, 255, 255}},
        {"Blood Ledger", "+More spell power. Downside: every room starts cursed.",
         EquipmentSlot::Trinket, SDL_Color{190, 45, 70, 255}},
        {"Borrowed Halo", "+Strong protection. Downside: debt collectors will come later.",
         EquipmentSlot::Relic, SDL_Color{235, 210, 120, 255}},
    };
    for (int i = 0; i < 7 && i < kInventorySlotCount; ++i) {
        m_inventorySlots[i] = i;
    }
}

SDL_Rect Game::inventoryBagSlotRect(int index) const {
    constexpr int kSlot = 64;
    constexpr int kGap = 10;
    const int col = index % kInventoryColumns;
    const int row = index / kInventoryColumns;
    const int startX = m_width / 2 + 40;
    const int startY = 190;
    return SDL_Rect{startX + col * (kSlot + kGap),
                    startY + row * (kSlot + kGap),
                    kSlot, kSlot};
}

SDL_Rect Game::inventoryEquipmentSlotRect(int index) const {
    constexpr int kSlot = 64;
    constexpr int kGap = 18;
    const int startX = m_width / 2 - 390;
    const int startY = 190;
    return SDL_Rect{startX, startY + index * (kSlot + kGap), kSlot, kSlot};
}

int Game::inventorySlotAt(int mouseX, int mouseY, bool* equipment) const {
    auto contains = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };
    for (int i = 0; i < kEquipmentSlotCount; ++i) {
        if (contains(inventoryEquipmentSlotRect(i))) {
            if (equipment) *equipment = true;
            return i;
        }
    }
    for (int i = 0; i < kInventorySlotCount; ++i) {
        if (contains(inventoryBagSlotRect(i))) {
            if (equipment) *equipment = false;
            return i;
        }
    }
    return -1;
}

bool Game::inventorySlotAccepts(bool equipment, int slot, int itemIndex) const {
    if (itemIndex < 0) {
        return true;
    }
    if (itemIndex >= static_cast<int>(m_inventoryItems.size())) {
        return false;
    }
    if (!equipment) {
        return slot >= 0 && slot < kInventorySlotCount;
    }
    if (slot < 0 || slot >= kEquipmentSlotCount) {
        return false;
    }
    const EquipmentSlot slotType = static_cast<EquipmentSlot>(slot);
    return m_inventoryItems[itemIndex].equipSlot == slotType;
}

bool Game::moveInventoryItem(bool fromEquipment, int fromSlot,
                             bool toEquipment, int toSlot) {
    if (fromEquipment == toEquipment && fromSlot == toSlot) {
        return true;
    }
    if (fromSlot < 0 || toSlot < 0) {
        return false;
    }
    int& from = fromEquipment ? m_equipmentSlots[fromSlot] : m_inventorySlots[fromSlot];
    int& to = toEquipment ? m_equipmentSlots[toSlot] : m_inventorySlots[toSlot];
    if (from < 0 || !inventorySlotAccepts(toEquipment, toSlot, from)) {
        return false;
    }
    if (to >= 0 && !inventorySlotAccepts(fromEquipment, fromSlot, to)) {
        return false;
    }
    const int moving = from;
    from = to;
    to = moving;
    saveProgress();
    return true;
}

bool Game::addInventoryItem(const InventoryItem& item) {
    for (int& slot : m_inventorySlots) {
        if (slot < 0) {
            m_inventoryItems.push_back(item);
            slot = static_cast<int>(m_inventoryItems.size()) - 1;
            return true;
        }
    }
    return false;
}

SDL_Rect Game::shadyShopItemRect(int index) const {
    constexpr int kCardW = 300;
    constexpr int kCardH = 270;
    constexpr int kGap = 24;
    const int totalW = 3 * kCardW + 2 * kGap;
    return SDL_Rect{m_width / 2 - totalW / 2 + index * (kCardW + kGap),
                    180,
                    kCardW,
                    kCardH};
}

SDL_Rect Game::consciousnessCardRect(int offset) const {
    if (offset == 0) {
        return SDL_Rect{m_width / 2 - 130, 190, 260, 310};
    }
    const int distance = std::abs(offset);
    const int w = distance == 1 ? 190 : 140;
    const int h = distance == 1 ? 230 : 170;
    const int centerX = m_width / 2 + offset * 250;
    const int centerY = distance == 1 ? 350 : 360;
    return SDL_Rect{centerX - w / 2, centerY - h / 2, w, h};
}

SDL_Rect Game::consciousnessLeftButtonRect() const {
    return SDL_Rect{m_width / 2 - 470, 314, 78, 72};
}

SDL_Rect Game::consciousnessRightButtonRect() const {
    return SDL_Rect{m_width / 2 + 392, 314, 78, 72};
}

SDL_Rect Game::artifactCardRect(int index) const {
    constexpr int kCardW = 340;
    constexpr int kCardH = 158;
    constexpr int kGap = 28;
    constexpr int kRowGap = 12;
    const int col = index % kArtifactColumns;
    const int row = index / kArtifactColumns;
    const int totalW = kArtifactColumns * kCardW + (kArtifactColumns - 1) * kGap;
    return SDL_Rect{m_width / 2 - totalW / 2 + col * (kCardW + kGap),
                    126 + row * (kCardH + kRowGap),
                    kCardW,
                    kCardH};
}

void Game::rotateCharacterSelection(int direction) {
    if (direction == 0) {
        return;
    }
    const int count = static_cast<int>(m_characterOptions.size());
    for (int step = 1; step <= count; ++step) {
        int candidate = (m_selectedCharacterIndex + direction * step) % count;
        if (candidate < 0) {
            candidate += count;
        }
        if (m_characterOptions[static_cast<std::size_t>(candidate)].unlocked) {
            m_selectedCharacterIndex = candidate;
            if (m_player) {
                m_player->setCharacterKind(selectedCharacter().texture);
            }
            saveProgress();
            return;
        }
    }
}

const Game::CharacterOption& Game::selectedCharacter() const {
    return m_characterOptions[static_cast<std::size_t>(m_selectedCharacterIndex)];
}

void Game::awardEnemyCurrencies(int killedCount) {
    if (killedCount <= 0) {
        return;
    }
    std::uniform_int_distribution<int> goldRoll(2, 6);
    std::uniform_int_distribution<int> shadyDropRoll(1, 100);
    for (int i = 0; i < killedCount; ++i) {
        int gold = goldRoll(m_currencyRng);
        if (m_selectedArtifactIndex == 0) {
            gold += (gold + 1) / 2;
        }
        m_gold += gold;
        if (shadyDropRoll(m_currencyRng) <= 8) {
            ++m_shadyCurrency;
        }
    }
}

bool Game::selectArtifact(int index) {
    if (index < 0 || index >= static_cast<int>(m_artifactOptions.size())) {
        return false;
    }
    ArtifactOption& artifact = m_artifactOptions[static_cast<std::size_t>(index)];
    if (!artifact.unlocked || artifact.itemIndex < 0) {
        return false;
    }
    m_selectedArtifactIndex = index;
    m_equipmentSlots[static_cast<int>(EquipmentSlot::Artifact)] = artifact.itemIndex;
    saveProgress();
    return true;
}

SDL_Rect Game::runeConfigActiveSlotRect(int index) const {
    constexpr int kRowH = 34;
    return SDL_Rect{m_width / 2 - 470, 154 + index * (kRowH + 6), 390, kRowH};
}

SDL_Rect Game::runeConfigLibrarySlotRect(int index) const {
    constexpr int kRowH = 30;
    return SDL_Rect{m_width / 2 - 30,
                    154 + index * (kRowH + 6),
                    545,
                    kRowH};
}

SDL_Rect Game::runeResearchButtonRect() const {
    return SDL_Rect{m_width / 2 - 170, m_height - 58, 340, 40};
}

SDL_Rect Game::runeResearchInventoryRuneRect(int index) const {
    constexpr int kRowH = 58;
    return SDL_Rect{m_width / 2 - 500, 178 + index * (kRowH + 12), 380, kRowH};
}

SDL_Rect Game::runeResearchExtractionSlotRect() const {
    return SDL_Rect{m_width / 2 - 72, 250, 144, 144};
}

int Game::runeConfigActiveSlotAt(int mouseX, int mouseY) const {
    auto contains = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };
    const int count = static_cast<int>(spellLoadoutIndices().size());
    for (int i = 0; i < count; ++i) {
        if (contains(runeConfigActiveSlotRect(i))) {
            return i;
        }
    }
    return -1;
}

int Game::runeConfigLibrarySlotAt(int mouseX, int mouseY) const {
    auto contains = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };
    std::vector<int> researched;
    for (int i = 0; i < static_cast<int>(allSpellRegistry().size()); ++i) {
        if (isSpellResearched(i)) {
            researched.push_back(i);
        }
    }
    const int maxScroll = static_cast<int>(researched.size()) > kRuneLibraryVisibleCount
        ? static_cast<int>(researched.size()) - kRuneLibraryVisibleCount
        : 0;
    const int scrollOffset = clampInt(m_runeLibraryScrollOffset, 0, maxScroll);
    for (int visibleSlot = 0; visibleSlot < kRuneLibraryVisibleCount; ++visibleSlot) {
        const int researchedSlot = scrollOffset + visibleSlot;
        if (researchedSlot >= static_cast<int>(researched.size())) break;
        if (contains(runeConfigLibrarySlotRect(visibleSlot))) {
            return researched[static_cast<std::size_t>(researchedSlot)];
        }
    }
    return -1;
}

int Game::runeResearchInventoryRuneAt(int mouseX, int mouseY) const {
    auto contains = [&](const SDL_Rect& r) {
        return mouseX >= r.x && mouseX < r.x + r.w &&
               mouseY >= r.y && mouseY < r.y + r.h;
    };
    int runeSlot = 0;
    for (int i = 0; i < static_cast<int>(m_inventoryItems.size()); ++i) {
        if (m_inventoryItems[static_cast<std::size_t>(i)].kind != InventoryItemKind::Rune) {
            continue;
        }
        if (contains(runeResearchInventoryRuneRect(runeSlot))) {
            return i;
        }
        ++runeSlot;
    }
    return -1;
}

bool Game::isSpellResearched(int spellIndex) const {
    return spellIndex >= 0 &&
           spellIndex < static_cast<int>(m_researchedSpells.size()) &&
           m_researchedSpells[static_cast<std::size_t>(spellIndex)];
}

bool Game::extractRuneItem(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_inventoryItems.size())) {
        return false;
    }
    InventoryItem& item = m_inventoryItems[static_cast<std::size_t>(itemIndex)];
    if (item.kind != InventoryItemKind::Rune || item.extracted) {
        return false;
    }
    for (int i = 0; i < item.containedSpellCount; ++i) {
        const int spellIndex = item.containedSpells[static_cast<std::size_t>(i)];
        if (spellIndex >= 0 && spellIndex < static_cast<int>(m_researchedSpells.size())) {
            m_researchedSpells[static_cast<std::size_t>(spellIndex)] = true;
        }
    }
    item.extracted = true;
    saveProgress();
    return true;
}

void Game::renderRuneConfig() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 8, 12, 18, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white{255, 255, 255, 255};
    const SDL_Color hint{170, 185, 205, 255};
    const SDL_Color cyan{120, 240, 255, 255};
    const SDL_Color gold{255, 210, 90, 255};
    drawTextCentered("Rune Configuration Engine", m_width / 2, 46, cyan);
    drawTextCentered("Drag researched runes into active spellbook slots to socket them.",
                     m_width / 2, 78, hint);
    drawTextCentered("Press E or Esc to close.", m_width / 2, 106, hint);

    const SDL_Point mouse = mouseLogical();
    auto hovered = [&](const SDL_Rect& r) {
        return mouse.x >= r.x && mouse.x < r.x + r.w &&
               mouse.y >= r.y && mouse.y < r.y + r.h;
    };
    const int hoveredActiveSlot = runeConfigActiveSlotAt(mouse.x, mouse.y);
    const int hoveredLibrarySlot = runeConfigLibrarySlotAt(mouse.x, mouse.y);

    auto drawPanelRow = [&](const SDL_Rect& r, SDL_Color border, bool selected) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, selected ? 42 : 24,
                               selected ? 58 : 34,
                               selected ? 74 : 46, 235);
        SDL_RenderFillRect(m_renderer, &r);
        SDL_SetRenderDrawColor(m_renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(m_renderer, &r);
    };

    const SDL_Rect researchButton = runeResearchButtonRect();
    drawPanelRow(researchButton, m_runeResearchOpen ? gold : cyan, m_runeResearchOpen);
    drawTextCentered(m_runeResearchOpen ? "Return to Spell Socketing" : "Open Rune Research",
                     researchButton.x + researchButton.w / 2,
                     researchButton.y + researchButton.h / 2,
                     m_runeResearchOpen ? gold : white);

    if (m_runeResearchOpen) {
        drawTextCentered("Rune Research", m_width / 2, 130, gold);
        drawTextCentered("Drag an inventory rune into the engine core to extract its contained spells.",
                         m_width / 2, 158, hint);

        drawTextCentered("Inventory Runes", m_width / 2 - 310, 196, gold);
        int runeSlot = 0;
        for (int i = 0; i < static_cast<int>(m_inventoryItems.size()); ++i) {
            const InventoryItem& item = m_inventoryItems[static_cast<std::size_t>(i)];
            if (item.kind != InventoryItemKind::Rune) {
                continue;
            }
            const SDL_Rect row = runeResearchInventoryRuneRect(runeSlot++);
            const bool hover = hovered(row) && !item.extracted;
            drawPanelRow(row, item.extracted ? SDL_Color{82, 82, 90, 255}
                                             : (hover ? gold : SDL_Color{110, 150, 175, 255}),
                         hover);
            SDL_Rect icon{row.x + 12, row.y + 10, 38, 38};
            SDL_SetRenderDrawColor(m_renderer, item.color.r, item.color.g, item.color.b,
                                   item.extracted ? 100 : 255);
            SDL_RenderFillRect(m_renderer, &icon);
            SDL_SetRenderDrawColor(m_renderer, 230, 245, 255, item.extracted ? 120 : 255);
            SDL_RenderDrawRect(m_renderer, &icon);
            drawTextCentered(item.name, row.x + 210, row.y + 18,
                             item.extracted ? hint : white);
            drawTextCentered(item.extracted ? "Extracted" : "Drag to engine",
                             row.x + 210, row.y + 42,
                             item.extracted ? SDL_Color{120, 220, 150, 255} : hint);
        }

        const SDL_Rect slot = runeResearchExtractionSlotRect();
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, hovered(slot) ? 38 : 24,
                               hovered(slot) ? 70 : 48,
                               hovered(slot) ? 88 : 66, 240);
        SDL_RenderFillRect(m_renderer, &slot);
        SDL_SetRenderDrawColor(m_renderer, 120, 240, 255, 255);
        SDL_RenderDrawRect(m_renderer, &slot);
        drawTextCentered("ENGINE", slot.x + slot.w / 2, slot.y + 42, cyan);
        drawTextCentered("CORE", slot.x + slot.w / 2, slot.y + 72, cyan);
        drawTextCentered("Drop rune here", slot.x + slot.w / 2, slot.y + 112, hint);

        drawTextCentered("Extracted spells appear in the researched rune library.",
                         m_width / 2, 500, hint);

        if (m_draggedResearchRune.active &&
            m_draggedResearchRune.itemIndex >= 0 &&
            m_draggedResearchRune.itemIndex < static_cast<int>(m_inventoryItems.size())) {
            const InventoryItem& item =
                m_inventoryItems[static_cast<std::size_t>(m_draggedResearchRune.itemIndex)];
            const SDL_Rect dragged{mouse.x - 130, mouse.y - 22, 260, 44};
            drawPanelRow(dragged, cyan, true);
            drawTextCentered(item.name, dragged.x + dragged.w / 2,
                             dragged.y + dragged.h / 2, white);
        }

        drawCursor();
        presentFrame();
        return;
    }

    drawTextCentered("Active Spellbook Runes", m_width / 2 - 275, 130, gold);
    const std::vector<int>& loadout = spellLoadoutIndices();
    const std::vector<SpellDef>& all = allSpellRegistry();
    for (int i = 0; i < static_cast<int>(loadout.size()); ++i) {
        const SDL_Rect row = runeConfigActiveSlotRect(i);
        const bool selected = i == m_selectedRuneLoadoutSlot;
        const bool hovered = i == hoveredActiveSlot;
        drawPanelRow(row, selected || hovered ? cyan : SDL_Color{90, 110, 130, 255},
                     selected || hovered);
        const int spellIndex = loadout[static_cast<std::size_t>(i)];
        std::string label = std::to_string(i + 1) + ". ";
        if (spellIndex >= 0 && spellIndex < static_cast<int>(all.size())) {
            label += all[static_cast<std::size_t>(spellIndex)].name;
        } else {
            label += "Empty";
        }
        if (!(m_draggedRune.active && m_draggedRune.fromLoadout &&
              m_draggedRune.fromSlot == i)) {
            drawTextCentered(label.c_str(), row.x + row.w / 2, row.y + row.h / 2, white);
        }
    }

    std::vector<int> researchedLibrary;
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        if (isSpellResearched(i)) {
            researchedLibrary.push_back(i);
        }
    }
    const int maxRuneScroll =
        static_cast<int>(researchedLibrary.size()) > kRuneLibraryVisibleCount
            ? static_cast<int>(researchedLibrary.size()) - kRuneLibraryVisibleCount
            : 0;
    m_runeLibraryScrollOffset =
        clampInt(m_runeLibraryScrollOffset, 0, maxRuneScroll);

    drawTextCentered("Researched Rune Library", m_width / 2 + 242, 130, gold);
    for (int visibleSlot = 0; visibleSlot < kRuneLibraryVisibleCount; ++visibleSlot) {
        const int librarySlot = m_runeLibraryScrollOffset + visibleSlot;
        if (librarySlot >= static_cast<int>(researchedLibrary.size())) {
            break;
        }
        const int i = researchedLibrary[static_cast<std::size_t>(librarySlot)];
        const SpellDef& spell = all[static_cast<std::size_t>(i)];
        bool active = false;
        for (int activeIndex : loadout) {
            if (activeIndex == i) {
                active = true;
                break;
            }
        }
        const SDL_Rect row = runeConfigLibrarySlotRect(visibleSlot);
        drawPanelRow(row, active ? SDL_Color{88, 74, 46, 255}
                                 : SDL_Color{78, 112, 140, 255},
                     i == hoveredLibrarySlot);
        SDL_Color textColor = active ? SDL_Color{210, 195, 150, 255} : white;
        if (!(m_draggedRune.active && !m_draggedRune.fromLoadout &&
              m_draggedRune.fromSlot == i)) {
            drawTextCentered(spell.name, row.x + row.w / 2, row.y + row.h / 2, textColor);
        }
    }
    if (maxRuneScroll > 0) {
        const SDL_Rect first = runeConfigLibrarySlotRect(0);
        const SDL_Rect last = runeConfigLibrarySlotRect(kRuneLibraryVisibleCount - 1);
        const SDL_Rect track{last.x + last.w + 12, first.y, 9,
                             last.y + last.h - first.y};
        const int thumbH = track.h * kRuneLibraryVisibleCount /
                           static_cast<int>(researchedLibrary.size());
        const int thumbY = track.y +
                           (track.h - thumbH) * m_runeLibraryScrollOffset / maxRuneScroll;
        const SDL_Rect thumb{track.x, thumbY, track.w, thumbH};
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 35, 48, 62, 210);
        SDL_RenderFillRect(m_renderer, &track);
        SDL_SetRenderDrawColor(m_renderer, 120, 240, 255, 235);
        SDL_RenderFillRect(m_renderer, &thumb);
        drawTextCentered("Mouse wheel / Up-Down to scroll",
                         m_width / 2 + 242, 526, hint);
    }

    int detailIndex = -1;
    if (!m_draggedRune.active) {
        if (hoveredLibrarySlot >= 0) {
            detailIndex = hoveredLibrarySlot;
        } else if (hoveredActiveSlot >= 0 &&
                   hoveredActiveSlot < static_cast<int>(loadout.size())) {
            detailIndex = loadout[static_cast<std::size_t>(hoveredActiveSlot)];
        }
    }
    if (detailIndex >= 0 && detailIndex < static_cast<int>(all.size())) {
        const SpellDef& spell = all[static_cast<std::size_t>(detailIndex)];
        const SDL_Rect panel{m_width / 2 - 470, m_height - 172, 940, 82};
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 18, 24, 34, 235);
        SDL_RenderFillRect(m_renderer, &panel);
        SDL_SetRenderDrawColor(m_renderer, 90, 130, 160, 255);
        SDL_RenderDrawRect(m_renderer, &panel);
        drawTextCentered(spell.name, panel.x + panel.w / 2, panel.y + 22, cyan);
        std::string details = std::string(spell.description) +
            " Damage " + std::to_string(spell.damage) +
            " | " + (spell.pierces ? "Piercing" : "Impact");
        drawTextCentered(details.c_str(), panel.x + panel.w / 2,
                         panel.y + 52, hint);
    }

    if (m_draggedRune.active &&
        m_draggedRune.spellIndex >= 0 &&
        m_draggedRune.spellIndex < static_cast<int>(all.size())) {
        const SpellDef& spell = all[static_cast<std::size_t>(m_draggedRune.spellIndex)];
        const SDL_Rect dragged{mouse.x - 120, mouse.y - 16, 240, 32};
        drawPanelRow(dragged, cyan, true);
        drawTextCentered(spell.name, dragged.x + dragged.w / 2,
                         dragged.y + dragged.h / 2, white);
    }

    drawCursor();
    presentFrame();
}

void Game::renderShadyShop() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 9, 8, 12, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white{245, 240, 225, 255};
    const SDL_Color hint{180, 168, 145, 255};
    const SDL_Color gold{235, 190, 95, 255};
    const SDL_Color red{235, 90, 95, 255};
    drawTextCentered("Shady Dealer", m_width / 2, 58, gold);
    drawTextCentered("One item only. Each deal costs 1 Shady Token.",
                     m_width / 2, 92, hint);
    drawTextCentered("Press E or Esc to walk away.", m_width / 2, 122, hint);

    const SDL_Point mouse = mouseLogical();
    auto hovered = [&](const SDL_Rect& r) {
        return mouse.x >= r.x && mouse.x < r.x + r.w &&
               mouse.y >= r.y && mouse.y < r.y + r.h;
    };

    for (int i = 0; i < static_cast<int>(m_shadyShopItems.size()); ++i) {
        const InventoryItem& item = m_shadyShopItems[static_cast<std::size_t>(i)];
        const SDL_Rect card = shadyShopItemRect(i);
        const bool hover = hovered(card) && !m_shadyPurchaseMade;
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, hover ? 48 : 28,
                               hover ? 42 : 28,
                               hover ? 44 : 34, 245);
        SDL_RenderFillRect(m_renderer, &card);
        SDL_SetRenderDrawColor(m_renderer, hover ? 250 : 120,
                               hover ? 205 : 95,
                               hover ? 110 : 60, 255);
        SDL_RenderDrawRect(m_renderer, &card);

        SDL_Rect icon{card.x + card.w / 2 - 28, card.y + 26, 56, 56};
        SDL_SetRenderDrawColor(m_renderer, item.color.r, item.color.g,
                               item.color.b, m_shadyPurchaseMade ? 120 : 255);
        SDL_RenderFillRect(m_renderer, &icon);
        SDL_SetRenderDrawColor(m_renderer, 255, 245, 210, 255);
        SDL_RenderDrawRect(m_renderer, &icon);

        drawTextCentered(item.name, card.x + card.w / 2, card.y + 108,
                         m_shadyPurchaseMade ? hint : white);
        drawTextCentered(equipmentSlotName(item.equipSlot),
                         card.x + card.w / 2, card.y + 138, gold);
        drawTextWrappedCentered(item.description, card.x + card.w / 2,
                                card.y + 188, card.w - 32, red);
        if (!m_shadyPurchaseMade) {
            drawTextCentered(m_shadyCurrency > 0 ? "Click to buy" : "Need 1 Shady Token",
                             card.x + card.w / 2,
                             card.y + card.h - 18, hint);
        }
    }

    if (m_shadyPurchaseMade) {
        drawTextCentered("Deal sealed. Your chosen item is in your inventory for the next run.",
                         m_width / 2, m_height - 96, gold);
    }

    drawCursor();
    presentFrame();
}

void Game::renderConsciousnessConsole() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 7, 8, 15, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white{245, 245, 255, 255};
    const SDL_Color hint{170, 168, 205, 255};
    const SDL_Color violet{190, 165, 255, 255};
    const SDL_Color locked{135, 135, 145, 255};

    drawTextCentered("Consciousness Transferral Console", m_width / 2, 54, violet);
    drawTextCentered("Cycle vessels with Left/Right or the arrows. Press E or Esc to close.",
                     m_width / 2, 88, hint);

    const SDL_Point mouse = mouseLogical();
    auto hovered = [&](const SDL_Rect& r) {
        return mouse.x >= r.x && mouse.x < r.x + r.w &&
               mouse.y >= r.y && mouse.y < r.y + r.h;
    };

    for (int offset : {-2, -1, 1, 2}) {
        const int count = static_cast<int>(m_characterOptions.size());
        int index = (m_selectedCharacterIndex + offset) % count;
        if (index < 0) {
            index += count;
        }
        const CharacterOption& option =
            m_characterOptions[static_cast<std::size_t>(index)];
        const SDL_Rect card = consciousnessCardRect(offset);
        const bool near = std::abs(offset) == 1;
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, option.unlocked ? 22 : 18,
                               option.unlocked ? 24 : 18,
                               option.unlocked ? 42 : 22,
                               near ? 225 : 170);
        SDL_RenderFillRect(m_renderer, &card);
        SDL_SetRenderDrawColor(m_renderer,
                               option.unlocked ? 95 : 75,
                               option.unlocked ? 85 : 75,
                               option.unlocked ? 145 : 85,
                               near ? 255 : 180);
        SDL_RenderDrawRect(m_renderer, &card);

        SDL_Texture* tex = m_textures ? m_textures->defaultFor(option.texture) : nullptr;
        if (tex) {
            const int sprite = near ? 86 : 62;
            const SDL_Rect dst{card.x + card.w / 2 - sprite / 2,
                               card.y + 32,
                               sprite,
                               sprite};
            SDL_SetTextureAlphaMod(tex, option.unlocked ? 205 : 135);
            SDL_RenderCopyEx(m_renderer, tex, nullptr, &dst, 0.0, nullptr,
                             SDL_FLIP_NONE);
            SDL_SetTextureAlphaMod(tex, 255);
        }
        drawTextCentered(option.unlocked ? option.name : "Locked",
                         card.x + card.w / 2,
                         card.y + card.h - 48,
                         option.unlocked ? hint : locked);
        if (!option.unlocked) {
            drawTextCentered("???", card.x + card.w / 2,
                             card.y + card.h - 22, locked);
        }
    }

    const CharacterOption& selected = selectedCharacter();
    const SDL_Rect center = consciousnessCardRect(0);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 28, 26, 54, 245);
    SDL_RenderFillRect(m_renderer, &center);
    SDL_SetRenderDrawColor(m_renderer, 205, 180, 255, 255);
    SDL_RenderDrawRect(m_renderer, &center);

    SDL_Texture* tex = m_textures ? m_textures->defaultFor(selected.texture) : nullptr;
    if (tex) {
        const SDL_Rect dst{center.x + center.w / 2 - 72, center.y + 32, 144, 144};
        SDL_RenderCopyEx(m_renderer, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
    }
    drawTextCentered(selected.name, center.x + center.w / 2,
                     center.y + 205, white);
    drawTextWrappedCentered(selected.subtitle, center.x + center.w / 2,
                            center.y + 246, center.w - 36, hint);
    drawTextCentered("ACTIVE VESSEL", center.x + center.w / 2,
                     center.y + center.h - 24, violet);

    const SDL_Rect left = consciousnessLeftButtonRect();
    const SDL_Rect right = consciousnessRightButtonRect();
    auto drawButton = [&](const SDL_Rect& r, const char* text, bool isHover) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, isHover ? 48 : 26,
                               isHover ? 42 : 28,
                               isHover ? 76 : 50, 240);
        SDL_RenderFillRect(m_renderer, &r);
        SDL_SetRenderDrawColor(m_renderer, 180, 160, 255, 255);
        SDL_RenderDrawRect(m_renderer, &r);
        drawTextCentered(text, r.x + r.w / 2, r.y + r.h / 2, white);
    };
    drawButton(left, "<", hovered(left));
    drawButton(right, ">", hovered(right));

    drawTextCentered("The sixth vessel is present, but its unlock method is unknown.",
                     m_width / 2, m_height - 88, locked);

    drawCursor();
    presentFrame();
}

void Game::renderSkillTreeSelector() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 4, 12, 8, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color green{95, 255, 150, 255};
    const SDL_Color dimGreen{70, 170, 105, 255};
    const SDL_Color text{210, 255, 225, 255};
    const SDL_Color hint{135, 205, 160, 255};

    drawTextCentered("Skill Tree Selector", m_width / 2, 52, green);
    drawTextCentered("Hologram preview only - no nodes can be selected yet.",
                     m_width / 2, 86, hint);
    drawTextCentered("Press E or Esc to close.", m_width / 2, 116, hint);

    const SDL_Rect panel{m_width / 2 - 360, 150, 720, 500};
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 6, 34, 20, 225);
    SDL_RenderFillRect(m_renderer, &panel);
    SDL_SetRenderDrawColor(m_renderer, 70, 255, 135, 220);
    SDL_RenderDrawRect(m_renderer, &panel);

    for (int y = panel.y + 30; y < panel.y + panel.h; y += 42) {
        SDL_SetRenderDrawColor(m_renderer, 25, 120, 65, 45);
        SDL_RenderDrawLine(m_renderer, panel.x + 18, y,
                           panel.x + panel.w - 18, y);
    }
    for (int x = panel.x + 30; x < panel.x + panel.w; x += 48) {
        SDL_SetRenderDrawColor(m_renderer, 25, 120, 65, 35);
        SDL_RenderDrawLine(m_renderer, x, panel.y + 18,
                           x, panel.y + panel.h - 18);
    }

    const std::array<SDL_Point, 13> nodes{{
        {m_width / 2, 570},
        {m_width / 2 - 120, 475}, {m_width / 2 + 120, 475},
        {m_width / 2 - 210, 380}, {m_width / 2 - 40, 380},
        {m_width / 2 + 40, 380}, {m_width / 2 + 210, 380},
        {m_width / 2 - 260, 285}, {m_width / 2 - 150, 285},
        {m_width / 2, 285},
        {m_width / 2 + 150, 285}, {m_width / 2 + 260, 285},
        {m_width / 2, 205},
    }};
    const std::array<std::pair<int, int>, 14> links{{
        {0, 1}, {0, 2},
        {1, 3}, {1, 4}, {2, 5}, {2, 6},
        {3, 7}, {3, 8}, {4, 9}, {5, 9}, {6, 10}, {6, 11},
        {9, 12}, {10, 12},
    }};

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    for (const auto& link : links) {
        const SDL_Point& a = nodes[static_cast<std::size_t>(link.first)];
        const SDL_Point& b = nodes[static_cast<std::size_t>(link.second)];
        SDL_SetRenderDrawColor(m_renderer, 60, 255, 140, 130);
        SDL_RenderDrawLine(m_renderer, a.x, a.y, b.x, b.y);
        SDL_SetRenderDrawColor(m_renderer, 155, 255, 195, 70);
        SDL_RenderDrawLine(m_renderer, a.x + 1, a.y, b.x + 1, b.y);
    }

    auto drawNode = [&](SDL_Point center, int radius, SDL_Color color) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, 55);
        for (int y = -radius - 8; y <= radius + 8; ++y) {
            for (int x = -radius - 8; x <= radius + 8; ++x) {
                if (x * x + y * y <= (radius + 8) * (radius + 8)) {
                    SDL_RenderDrawPoint(m_renderer, center.x + x, center.y + y);
                }
            }
        }
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= radius * radius) {
                    SDL_RenderDrawPoint(m_renderer, center.x + x, center.y + y);
                }
            }
        }
        SDL_SetRenderDrawColor(m_renderer, 225, 255, 235, 210);
        SDL_RenderDrawLine(m_renderer, center.x - radius + 3, center.y,
                           center.x + radius - 3, center.y);
        SDL_RenderDrawLine(m_renderer, center.x, center.y - radius + 3,
                           center.x, center.y + radius - 3);
    };

    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const int radius = (i == 0 || i == nodes.size() - 1) ? 18 : 13;
        drawNode(nodes[i], radius, i % 3 == 0 ? green : dimGreen);
    }

    drawTextCentered("ROOT", nodes[0].x, nodes[0].y + 42, text);
    drawTextCentered("ASCENDANT BRANCH", nodes[12].x, nodes[12].y - 36, text);

    drawCursor();
    presentFrame();
}

void Game::renderArtifactStorage() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 16, 10, 8, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white{255, 246, 226, 255};
    const SDL_Color hint{205, 180, 145, 255};
    const SDL_Color gold{255, 196, 90, 255};
    const SDL_Color locked{135, 125, 120, 255};

    drawTextCentered("Artifact Storage", m_width / 2, 42, gold);
    drawTextCentered("Select one permanently-unlocked artifact for a run-wide upgrade.",
                     m_width / 2, 72, hint);
    drawTextCentered("Mouse wheel or arrow keys to scroll. Press E or Esc to close.",
                     m_width / 2, 100, hint);

    const SDL_Point mouse = mouseLogical();
    auto hovered = [&](const SDL_Rect& r) {
        return mouse.x >= r.x && mouse.x < r.x + r.w &&
               mouse.y >= r.y && mouse.y < r.y + r.h;
    };

    for (int slot = 0; slot < kArtifactVisibleCount; ++slot) {
        const int i = m_artifactScrollOffset + slot;
        if (i >= static_cast<int>(m_artifactOptions.size())) {
            break;
        }
        const ArtifactOption& artifact = m_artifactOptions[static_cast<std::size_t>(i)];
        const SDL_Rect card = artifactCardRect(slot);
        const bool selected = i == m_selectedArtifactIndex;
        const bool hover = hovered(card) && artifact.unlocked;
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer,
                               artifact.unlocked ? (hover ? 58 : 36) : 28,
                               artifact.unlocked ? (hover ? 42 : 30) : 26,
                               artifact.unlocked ? (hover ? 28 : 22) : 24,
                               242);
        SDL_RenderFillRect(m_renderer, &card);
        SDL_SetRenderDrawColor(m_renderer,
                               selected ? 255 : (artifact.unlocked ? 150 : 80),
                               selected ? 210 : (artifact.unlocked ? 115 : 72),
                               selected ? 95 : (artifact.unlocked ? 70 : 68),
                               255);
        SDL_RenderDrawRect(m_renderer, &card);

        const SDL_Rect icon{card.x + 18, card.y + 18, 52, 52};
        SDL_SetRenderDrawColor(m_renderer,
                               artifact.unlocked ? artifact.color.r : 78,
                               artifact.unlocked ? artifact.color.g : 78,
                               artifact.unlocked ? artifact.color.b : 82,
                               255);
        SDL_RenderFillRect(m_renderer, &icon);
        SDL_SetRenderDrawColor(m_renderer, 255, 235, 180, artifact.unlocked ? 255 : 130);
        SDL_RenderDrawRect(m_renderer, &icon);
        if (!artifact.unlocked) {
            drawTextCentered("?", icon.x + icon.w / 2, icon.y + icon.h / 2, locked);
        }

        drawTextCentered(artifact.unlocked ? artifact.name : "Locked Artifact",
                         card.x + 210, card.y + 32,
                         artifact.unlocked ? white : locked);
        drawTextWrappedCentered(artifact.unlocked ? artifact.description
                                                 : artifact.challenge,
                                card.x + card.w / 2, card.y + 96,
                                card.w - 36,
                                artifact.unlocked ? hint : locked);
        drawTextCentered(selected ? "EQUIPPED" :
                         (artifact.unlocked ? "Click to equip" : "Challenge locked"),
                         card.x + card.w / 2, card.y + card.h - 22,
                         selected ? gold : (artifact.unlocked ? hint : locked));
    }

    const int maxScrollOffset =
        static_cast<int>(m_artifactOptions.size()) > kArtifactVisibleCount
            ? static_cast<int>(m_artifactOptions.size()) - kArtifactVisibleCount
            : 0;
    if (maxScrollOffset > 0) {
        const SDL_Rect firstCard = artifactCardRect(0);
        const SDL_Rect lastCard = artifactCardRect(kArtifactVisibleCount - 1);
        const SDL_Rect track{lastCard.x + lastCard.w + 18,
                             firstCard.y,
                             10,
                             lastCard.y + lastCard.h - firstCard.y};
        const int thumbH = track.h * kArtifactVisibleCount /
                           static_cast<int>(m_artifactOptions.size());
        const int thumbY = track.y +
                           (track.h - thumbH) * m_artifactScrollOffset / maxScrollOffset;
        const SDL_Rect thumb{track.x, thumbY, track.w, thumbH};
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 72, 54, 42, 190);
        SDL_RenderFillRect(m_renderer, &track);
        SDL_SetRenderDrawColor(m_renderer, 255, 196, 90, 235);
        SDL_RenderFillRect(m_renderer, &thumb);
    }

    const ArtifactOption& selected =
        m_artifactOptions[static_cast<std::size_t>(m_selectedArtifactIndex)];
    drawTextCentered((std::string("Active artifact: ") + selected.name).c_str(),
                     m_width / 2, m_height - 56, gold);
    drawTextCentered("Artifact challenge completion and persistence will be wired into future run goals.",
                     m_width / 2, m_height - 28, hint);

    drawCursor();
    presentFrame();
}

void Game::renderInventory() {
    beginFrame();
    SDL_SetRenderDrawColor(m_renderer, 10, 12, 18, 255);
    SDL_RenderClear(m_renderer);

    const SDL_Color white{255, 255, 255, 255};
    const SDL_Color hint{170, 175, 185, 255};
    const SDL_Color gold{255, 210, 90, 255};
    drawTextCentered("Inventory", m_width / 2, 54, white);
    drawTextCentered("Drag items between bag slots. Equipment only accepts matching item types.",
                     m_width / 2, 84, hint);
    drawTextCentered("Press I or Esc to close.", m_width / 2, 112, hint);

    const SDL_Point mouse = mouseLogical();
    auto hovered = [&](const SDL_Rect& r) {
        return mouse.x >= r.x && mouse.x < r.x + r.w &&
               mouse.y >= r.y && mouse.y < r.y + r.h;
    };

    auto drawSlot = [&](const SDL_Rect& slot, bool hover, bool equipment) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        if (hover) {
            SDL_SetRenderDrawColor(m_renderer, 72, 84, 108, 255);
        } else if (equipment) {
            SDL_SetRenderDrawColor(m_renderer, 44, 42, 56, 255);
        } else {
            SDL_SetRenderDrawColor(m_renderer, 34, 38, 48, 255);
        }
        SDL_RenderFillRect(m_renderer, &slot);
        SDL_SetRenderDrawColor(m_renderer, equipment ? 160 : 90,
                               equipment ? 125 : 100,
                               equipment ? 70 : 120, 255);
        SDL_RenderDrawRect(m_renderer, &slot);
    };

    auto drawItem = [&](int itemIndex, const SDL_Rect& slot, bool dragged) {
        if (itemIndex < 0 || itemIndex >= static_cast<int>(m_inventoryItems.size())) {
            return;
        }
        const InventoryItem& item = m_inventoryItems[itemIndex];
        SDL_Rect icon{slot.x + 8, slot.y + 8, slot.w - 16, slot.h - 16};
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, item.color.r, item.color.g,
                               item.color.b, dragged ? 190 : 255);
        SDL_RenderFillRect(m_renderer, &icon);
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, dragged ? 180 : 255);
        SDL_RenderDrawRect(m_renderer, &icon);
        if (item.kind == InventoryItemKind::Rune) {
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, dragged ? 170 : 230);
            SDL_RenderDrawLine(m_renderer, icon.x + 12, icon.y + icon.h / 2,
                               icon.x + icon.w - 12, icon.y + icon.h / 2);
            SDL_RenderDrawLine(m_renderer, icon.x + icon.w / 2, icon.y + 12,
                               icon.x + icon.w / 2, icon.y + icon.h - 12);
        }
    };

    drawTextCentered("Equipment", m_width / 2 - 360, 156, gold);
    for (int i = 0; i < kEquipmentSlotCount; ++i) {
        const SDL_Rect slot = inventoryEquipmentSlotRect(i);
        drawSlot(slot, hovered(slot), true);
        const EquipmentSlot slotType = static_cast<EquipmentSlot>(i);
        drawTextCentered(equipmentSlotName(slotType), slot.x + slot.w + 78,
                         slot.y + slot.h / 2, hint);
        const bool isDragSource = m_draggedInventoryItem.active &&
            m_draggedInventoryItem.fromEquipment &&
            m_draggedInventoryItem.fromSlot == i;
        if (!isDragSource) {
            drawItem(m_equipmentSlots[i], slot, false);
        }
    }

    drawTextCentered("Bag", m_width / 2 + 225, 156, gold);
    for (int i = 0; i < kInventorySlotCount; ++i) {
        const SDL_Rect slot = inventoryBagSlotRect(i);
        drawSlot(slot, hovered(slot), false);
        const bool isDragSource = m_draggedInventoryItem.active &&
            !m_draggedInventoryItem.fromEquipment &&
            m_draggedInventoryItem.fromSlot == i;
        if (!isDragSource) {
            drawItem(m_inventorySlots[i], slot, false);
        }
    }

    if (m_draggedInventoryItem.active) {
        const SDL_Rect dragged{mouse.x - 32, mouse.y - 32, 64, 64};
        drawItem(m_draggedInventoryItem.itemIndex, dragged, true);
    }

    int detailItem = m_draggedInventoryItem.active
        ? m_draggedInventoryItem.itemIndex
        : -1;
    if (detailItem < 0) {
        bool equipment = false;
        const int slot = inventorySlotAt(mouse.x, mouse.y, &equipment);
        if (slot >= 0) {
            detailItem = equipment ? m_equipmentSlots[slot] : m_inventorySlots[slot];
        }
    }
    if (detailItem >= 0 && detailItem < static_cast<int>(m_inventoryItems.size())) {
        const InventoryItem& item = m_inventoryItems[detailItem];
        const SDL_Rect panel{m_width / 2 - 360, m_height - 110, 720, 70};
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 24, 28, 38, 230);
        SDL_RenderFillRect(m_renderer, &panel);
        SDL_SetRenderDrawColor(m_renderer, 90, 100, 120, 255);
        SDL_RenderDrawRect(m_renderer, &panel);
        drawTextCentered(item.name, panel.x + panel.w / 2, panel.y + 22, white);
        const std::string itemDetails = item.kind == InventoryItemKind::Rune
            ? std::string(item.description) + (item.extracted ? " Status: extracted." : " Status: unresearched.")
            : std::string(item.description) + " Slot: " + equipmentSlotName(item.equipSlot);
        drawTextCentered(itemDetails.c_str(), panel.x + panel.w / 2,
                         panel.y + 50, hint);
    }

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
