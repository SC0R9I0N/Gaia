# Gaia

A minimal, from-scratch C++ game starter template. No game engine — just
standard C++20 and [SDL2](https://www.libsdl.org/) for cross-platform windowing,
input, and 2D rendering.

It compiles and runs out of the box, opening a window with a controllable
character on a grid backdrop — proving the full game loop
(init → update → render → shutdown) is working. Use it as the foundation for
your own game.

All visuals are **procedurally-generated placeholder textures** (see
`PlaceholderTextures`), so the game renders visible stand-ins for characters,
items, and backgrounds without needing any art assets.

## Project layout

```
Gaia/
├── CMakeLists.txt              # Build config; fetches & builds SDL2 automatically
├── include/
│   ├── Game.hpp               # Game class declaration
│   ├── Player.hpp            # Playable character: movement + actions
│   └── PlaceholderTextures.hpp # Procedural stand-in textures
├── src/
│   ├── main.cpp              # Entry point — creates and runs the Game
│   ├── Game.cpp             # Game loop implementation
│   ├── Player.cpp          # Player movement, roll, attack, item use
│   └── PlaceholderTextures.cpp # Texture generation
├── assets/                # Textures, audio, fonts... (copied next to the binary)
└── build/                # Generated build output (git-ignored)
```

## Requirements

- A C++20 compiler (MSVC, Clang, or GCC)
- [CMake](https://cmake.org/) 3.16+
- Git (CMake uses it to fetch SDL2)

No separate SDL2 install is needed — CMake downloads and builds it on the first
configure (this makes the initial build take a few minutes).

## Building & running

### CLion

Open the project folder. CLion detects `CMakeLists.txt` automatically — just
press the green **Run** button (or Shift+F10).

### Command line

```sh
cmake -S . -B build
cmake --build build
```

Then run the executable:

- Windows: `build\bin\Gaia.exe`
- macOS / Linux: `./build/bin/Gaia`

## Controls

- **W / A / S / D** — move the character
- **Space** — roll / dodge (speed burst + brief invulnerability; the sprite
  blinks during i-frames)
- **Left mouse button** — melee attack (opens a hitbox in front of you for a
  short window)
- **E** — use item (placeholder effect: a short shield + expanding ring)
- **Esc** or the window close button — quit

## Where to go next

- `Player` in `src/Player.cpp` — movement and the roll/attack/item actions
  (tunable constants live at the top of `include/Player.hpp`)
- `PlaceholderTextures` in `src/PlaceholderTextures.cpp` — swap the generated
  stand-ins for real `assets/*.png` art when you have it
- `Game::update()` / `Game::render()` / `Game::processEvents()` in
  `src/Game.cpp` — the per-frame loop, drawing, and input wiring
- Add new source files to the `add_executable(...)` list in `CMakeLists.txt`
