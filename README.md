# Gaia

A minimal, from-scratch C++ game starter template. No game engine — just
standard C++20 and [SDL2](https://www.libsdl.org/) for cross-platform windowing,
input, and 2D rendering.

It compiles and runs out of the box, opening a window with a controllable
character on a grid backdrop — proving the full game loop
(init → update → render → shutdown) is working. Use it as the foundation for
your own game.

Visuals load from simple **PNG sprites in `assets/`** (player, enemy, vendor,
floor, attack), with **procedurally-generated placeholders** as an automatic
fallback (see `PlaceholderTextures`). Artists can replace any sprite by dropping
a PNG into `assets/` — no code changes, no rebuild. See `assets/README.md` for
the file map and sizes.

## Known Issues

- Players are unable to fall in pits
- Enemies can phase through pits
- May still be some non-functionality with some specific settings (will need to thoroughly check)
- Combat is one-directional: enemies only chase and bump into the player, they
  don't deal damage. There is no player health, so the player can't take damage
  or die yet (runs have no fail state).
- All enemy types currently share the same "walk straight at the player" AI.
  Their class behaviours (ranged kiting, charging, bombing, summoning) and the
  abilities defined in `EnemyTypes.cpp` are data only — nothing fires them yet.
- When the player takes a door into a freshly generated room they appear against
  a blank wall; only the forward exit door is drawn, there is no entry doorway.
- Spells only resolve when the cast is released (Middle Mouse); there's no live
  feedback while entering a combo, and every spell renders the same placeholder
  projectile (a travelling red circle) regardless of which spell was cast.
- Vendors are decorative — they render but can't be interacted with. Vendor
  rooms show their wares (name + price per pedestal) but **buying is not
  implemented yet**, and the wares are placeholder items rolled per room rather
  than a curated, run-specific inventory.
- The item action (E) is a placeholder effect (a brief shield + ring), not a
  real inventory/item.

## Future Implementations

- Add more room layouts
- Make it so pits are rarely large enough that the player cannot dash across them
  - Make it so players can dash across/fall into pits
- Add more environmental hazards (Water that slows you, lava that burns, etc.)
- Wire up enemy AI and attacks using the per-type ability data in
  `EnemyTypes.cpp` (ranged shots, charges, explosions, summons) and give each
  enemy class its own movement behaviour
- Add a player health / damage / death system so combat is two-sided, plus a
  proper run win/lose flow and return-to-hub on completion
- Replace the random room picker with the planned fixed room "rotation" and add
  miniboss / boss rooms (the door-chaining system already supports this)
- Make vendor rooms functional: currency, buying the displayed items, and a
  curated per-run inventory (see "Vendor rooms" below for the spawn design and
  the parts still to wire up)
- Expand the spell system: distinct visuals/effects per spell, live combo
  feedback while casting, and more spells in the registry
- Loot / pickups and player progression (stats, leveling)
- Audio: sound effects and music
- Persistence: saving runs and meta-progression between sessions
- A minimap or room/progress indicator beyond the current "Room N" counter

### Vendor rooms

A vendor (shop) room is one of the possible run rooms. It has no enemies, so its
exit door is unlocked the moment you enter and you can browse and leave at will.
The shopkeeper displays a set of wares on pedestals (name + price), but **buying
is not implemented yet**.

How it spawns (implemented in `RoomSystem`):

- It is a **probability** spawn, never guaranteed by chance. The odds start very
  low and **rise with run depth**:
  `chance(depth) = min(kVendorChanceCap, kVendorBaseChance + kVendorChanceStep * depth)`
  (currently 0% base, +5% per room, capped at 60%).
- The run always **opens on a combat room** — the first room is never a vendor.
- **No two vendor rooms back-to-back**: at least one normal room must sit between
  them (tracked by `m_roomsSinceVendor`).

Designed but **not yet wired up** (noted here because the dependencies don't
exist yet):

- **Guaranteed vendor after a boss.** `RoomSystem::guaranteeVendorNextRoom()`
  already forces the next room to be a vendor; it just isn't called anywhere
  because boss rooms/kills don't exist yet. Hook it into the boss-death flow when
  bosses land.
- **Per-run inventory & buying.** Wares are currently rolled per vendor room from
  a placeholder catalogue. A real implementation would generate a run-specific
  inventory and let the player spend currency on the `ShopItem`s (the struct
  already carries `price`/`purchased`).

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
