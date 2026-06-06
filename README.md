# Gaia

A minimal, from-scratch C++ game starter template. No game engine — just
standard C++20 and [SDL2](https://www.libsdl.org/) for cross-platform windowing,
input, and 2D rendering.

It compiles and runs out of the box, opening a window with a bouncing box to
prove the full game loop (init → update → render → shutdown) is working. Use it
as the foundation for your own game.

## Project layout

```
Gaia/
├── CMakeLists.txt     # Build config; fetches & builds SDL2 automatically
├── include/
│   └── Game.hpp       # Game class declaration
├── src/
│   ├── main.cpp       # Entry point — creates and runs the Game
│   └── Game.cpp       # Game loop implementation
├── assets/            # Textures, audio, fonts... (copied next to the binary)
└── build/             # Generated build output (git-ignored)
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

- **Esc** or the window close button — quit

## Where to go next

- `Game::update()` in `src/Game.cpp` — your per-frame game logic
- `Game::render()` — your drawing code
- `Game::processEvents()` — keyboard/mouse/window input handling
- Add new source files to the `add_executable(...)` list in `CMakeLists.txt`
