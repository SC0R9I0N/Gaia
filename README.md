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

- Settings are still prototype-level and need a full pass for edge cases across
  resolution, display mode, focus, and frame-limit combinations.
- Combat has player health, death, enemy projectiles/area attacks, and spell
  impacts, but damage tuning, enemy hit feedback, and encounter balance are still
  rough.
- When the player takes a door into a freshly generated room they appear against
  a blank wall; only the forward exit door is drawn, there is no entry doorway.
- Spells only resolve when the cast is released (Middle Mouse); there's no live
  feedback while entering a combo.
- Spell VFX now use per-spell projectile/enemy-hit/wall-hit sprite sheets, but
  they are still placeholder-generated art and should be replaced or polished.
- The hub is a procedural/tile-style layout drawn in code, not a single authored
  map or external tilemap. This makes iteration quick but limits art direction.
- Generated vendor rooms show their wares (name + price per pedestal), but
  **buying is not implemented yet** there. Hub vendors are interactive: the rune
  engine changes active spells, the shady dealer sells one risky run item, the
  consciousness console swaps the active character texture, the skill tree
  selector previews a non-interactive hologram tree, and Artifact Storage equips
  one unlocked run artifact.
- Currency exists as gold for run purchases, skill currency for future skill
  points, and shady currency for shady-dealer purchases. Gold and rare shady
  tokens drop from enemy kills; skill currency is earned by clearing rooms.
- Meta-progression is saved between sessions, including researched spells,
  active spell loadout, selected character, artifact state, skill currency,
  shady currency, and inventory slot state. Abandoned runs roll back to their
  run-start snapshot, while death keeps meta-progression but drops gold and
  non-rune inventory items. This is still a text-file prototype save format.
- The item action (E) is a placeholder effect (a brief shield + ring), not a
  real inventory/item.

## Future Implementations

- Replace or extend the procedural hub with an authored hub map/tilemap so walls,
  lanterns, sandstone floors, and decorative ambience can be laid out by data
  instead of hard-coded draw calls.
- Add more run room layouts and room themes.
- Make pit generation more fair and add intentional pit traversal/fall rules.
- Add more environmental hazards (Water that slows you, lava that burns, etc.)
- Add live spell combo feedback while casting, plus clearer spell cooldown/timing
  feedback.
- Expand the run win/lose flow beyond death/exit returning to the hub, including
  rewards, summary screens, and a clearer distinction between abandon/death/win.
- Replace the random room picker with the planned fixed room "rotation" and add
  miniboss / boss rooms (the door-chaining system already supports this)
- Make vendor rooms functional: currency, buying the displayed items, and a
  curated per-run inventory (see "Vendor rooms" below for the spawn design and
  the parts still to wire up)
- Replace placeholder skill tree UI with functional skill trees and spendable
  skill-point progression.
- Tie character selection to character-specific stats, abilities, and skill
  trees instead of only texture/dash sprites.
- Implement artifact challenge completion and persistent artifact unlock rules.
- Add loot / pickups and fuller player progression (stats, leveling, run rewards)
- Audio: sound effects and music
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
- **E** — use item (or open the rune configuration engine, shady dealer,
  consciousness console, skill tree selector, or Artifact Storage when standing
  near them)
- **I** — open inventory (drag/drop bag items and equipment)
- **Esc** or the window close button — quit

## Where to go next

- `Player` in `src/Player.cpp` — movement and the roll/attack/item actions
  (tunable constants live at the top of `include/Player.hpp`)
- `PlaceholderTextures` in `src/PlaceholderTextures.cpp` — swap the generated
  stand-ins for real `assets/*.png` art when you have it
- `Game::update()` / `Game::render()` / `Game::processEvents()` in
  `src/Game.cpp` — the per-frame loop, drawing, and input wiring
- Add new source files to the `add_executable(...)` list in `CMakeLists.txt`
