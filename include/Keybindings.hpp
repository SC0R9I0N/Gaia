#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

namespace gaia {

// The set of player actions whose key can be changed in the settings menu.
//
// These are all keyboard bindings. The mouse-driven spell-cast sequence
// (left/right/middle to build a fireball/lightning) and Esc pause/menu backing
// are intentionally fixed.
enum class Action {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Roll,
    UseItem,
    Spellbook,
    Inventory,
    Count  // sentinel: number of actions, must stay last
};

// Maps each Action to an SDL scancode. Scancodes (not keycodes) are used so the
// same binding works both for polled movement (SDL_GetKeyboardState) and for
// one-shot key-down events (event.key.keysym.scancode).
class Keybindings {
public:
    Keybindings() { resetDefaults(); }

    SDL_Scancode get(Action action) const {
        return m_codes[static_cast<std::size_t>(action)];
    }
    void set(Action action, SDL_Scancode code) {
        m_codes[static_cast<std::size_t>(action)] = code;
    }

    // True while the key bound to this action is held down.
    bool isHeld(Action action, const Uint8* keyboard) const {
        return keyboard[get(action)] != 0;
    }

    // Restore the WASD / Space / E / Tab defaults.
    void resetDefaults() {
        set(Action::MoveUp,    SDL_SCANCODE_W);
        set(Action::MoveDown,  SDL_SCANCODE_S);
        set(Action::MoveLeft,  SDL_SCANCODE_A);
        set(Action::MoveRight, SDL_SCANCODE_D);
        set(Action::Roll,      SDL_SCANCODE_SPACE);
        set(Action::UseItem,   SDL_SCANCODE_E);
        set(Action::Spellbook, SDL_SCANCODE_TAB);
        set(Action::Inventory, SDL_SCANCODE_I);
    }

    // Human-readable action label, e.g. "Move Up".
    static const char* label(Action action) {
        switch (action) {
            case Action::MoveUp:    return "Move Up";
            case Action::MoveDown:  return "Move Down";
            case Action::MoveLeft:  return "Move Left";
            case Action::MoveRight: return "Move Right";
            case Action::Roll:      return "Roll / Dodge";
            case Action::UseItem:   return "Use Item";
            case Action::Spellbook: return "Spellbook";
            case Action::Inventory: return "Inventory";
            default:                return "";
        }
    }

    // Display name of the currently bound key, e.g. "W" or "Space".
    const char* keyName(Action action) const {
        const char* name = SDL_GetScancodeName(get(action));
        return (name && *name) ? name : "Unbound";
    }

    // ---- Persistence -------------------------------------------------------
    // Bindings are stored one-per-line as "<token> <scancode>" in a per-user
    // config file, so rebinds survive across sessions.

    // Writes the current bindings to the config file. Best-effort: failures
    // (e.g. unwritable path) are silently ignored.
    void save() const {
        const std::string path = configPath();
        std::FILE* f = std::fopen(path.c_str(), "w");
        if (!f) return;
        for (std::size_t i = 0; i < static_cast<std::size_t>(Action::Count); ++i) {
            const Action action = static_cast<Action>(i);
            std::fprintf(f, "%s %d\n", token(action),
                         static_cast<int>(get(action)));
        }
        std::fclose(f);
    }

    // Loads bindings from the config file, overriding defaults for any action
    // found there. Missing file or unknown/invalid entries leave defaults in
    // place, so it is always safe to call after resetDefaults().
    void load() {
        const std::string path = configPath();
        std::FILE* f = std::fopen(path.c_str(), "r");
        if (!f) return;  // no saved config yet
        char tok[64];
        int code = 0;
        while (std::fscanf(f, "%63s %d", tok, &code) == 2) {
            if (code < 0 || code >= SDL_NUM_SCANCODES) continue;
            for (std::size_t i = 0; i < static_cast<std::size_t>(Action::Count); ++i) {
                const Action action = static_cast<Action>(i);
                if (std::strcmp(token(action), tok) == 0) {
                    set(action, static_cast<SDL_Scancode>(code));
                    break;
                }
            }
        }
        std::fclose(f);
    }

private:
    // Stable, space-free identifier for an action, used as the config-file key.
    // Decoupled from label() so display text can change without breaking saves.
    static const char* token(Action action) {
        switch (action) {
            case Action::MoveUp:    return "MoveUp";
            case Action::MoveDown:  return "MoveDown";
            case Action::MoveLeft:  return "MoveLeft";
            case Action::MoveRight: return "MoveRight";
            case Action::Roll:      return "Roll";
            case Action::UseItem:   return "UseItem";
            case Action::Spellbook: return "Spellbook";
            case Action::Inventory: return "Inventory";
            default:                return "";
        }
    }

    // Full path to the per-user config file (directory is created by SDL).
    static std::string configPath() {
        std::string path = "keybinds.cfg";  // fallback: working directory
        char* base = SDL_GetPrefPath("Gaia", "Gaia");
        if (base) {
            path = std::string(base) + "keybinds.cfg";
            SDL_free(base);
        }
        return path;
    }

    std::array<SDL_Scancode, static_cast<std::size_t>(Action::Count)> m_codes{};
};

}  // namespace gaia
