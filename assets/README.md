# Assets

Drop game resources here — textures, audio, fonts, level data, etc.

This folder is copied next to the built executable after every build (see the
`POST_BUILD` step in the root `CMakeLists.txt`), so the game loads files at
runtime using a relative path such as `assets/player.png`.

## Sprites / textures (for artists)

The game renders everything through one texture provider,
`PlaceholderTextures` (`src/PlaceholderTextures.cpp`). For each kind of object it
tries to **load a PNG from this folder first**, and only falls back to a crude
procedurally-drawn stand-in if the file is missing. That means:

> **To replace any sprite, just drop a PNG with the matching name into this
> folder. No code changes, no rebuild — the game picks it up the next time it
> runs.**

The one place that maps a kind to its file is `assetFileName()` in
`src/PlaceholderTextures.cpp`; that function is the single source of truth.

### File map

| File          | What it is        | Authored size | In-game draw size | Notes |
|---------------|-------------------|---------------|-------------------|-------|
| `player.png`  | The player (Astral Wizard) | 24×24    | 48×48             | Static player sprite — author it **facing right (+x)**. The canonical quality bar for all vessels. |
| `character_2.png` … `character_6.png` | Alternate player vessels | 24×24 | 48×48 | Distinct archetypes (not reskinned wizards): **2** Dust Gunslinger (cowboy + revolver), **3** Iron Sentinel (knight + sword), **4** Veiled Rogue (hood + dagger), **5** Wild Ranger (archer + bow), **6** Unknown Vessel (locked shadow silhouette). Used by the consciousness transferral console. |
| `dash.png`    | Player teleport dodge | 24×24 / frame | 48×48             | **Sprite sheet** used while rolling/dashing for `player.png`; a magical blink/teleport (wizard silhouette). |
| `dash_2.png` … `dash_6.png` | Alternate vessel teleport dodges | 24×24 / frame | 48×48 | Per-vessel blink sheets that dissolve **that archetype's own silhouette** (gunslinger, knight, rogue, ranger, wraith) into its accent-coloured rift. |
| `enemy.png`   | Enemy creature    | 22×22         | varies            | **Author light/neutral** — the game tints it per enemy type (`EnemyTypes.cpp`) and scales it to each type's size. Flashes white on hit. |
| `vendor.png`  | Hub shop stall    | 32×40         | 64×80             | Static, upright. |
| `rune_vendor.png` | Rune engine vendor | 32×40     | 80×96             | Hub rune configuration engine used to swap active spellbook runes. |
| `shady_vendor.png` | Shady dealer | 32×40     | 80×96             | Hub vendor who sells one risky item for the next run. |
| `consciousness_console.png` | Consciousness console | 40×32 | 120×96 | Hub console used to rotate between playable character textures. |
| `skill_tree_console.png` | Skill tree selector | 32×40 | 80×96 | Hub console that previews a non-interactive hologram skill tree. |
| `artifact_storage.png` | Artifact Storage | 32×40 | 80×96 | Hub storage chest used to choose one unlocked run artifact. |
| `run_portal.png` | Run start portal | 48×48 / frame | 120×96 | **Sprite sheet** for the animated portal that starts a run. |
| `currency_gold.png` | Gold currency icon | 24×24 | 28×28 | In-run purchase currency earned from enemy kills. |
| `currency_skill.png` | Skill currency icon | 24×24 | 28×28 | Gradual skill-point currency earned from clearing rooms. |
| `currency_shady.png` | Shady currency icon | 24×24 | 28×28 | Rare shady-dealer currency, currently a rare enemy drop. |
| `floor.png`   | Room floor tile   | 32×32         | tiled at 96×96    | Must be **seamless** — it's repeated across the room. |
| `hub_floor.png` | Hub sandstone brick floor | 96×96 | tiled at 96×96 | Must be **seamless**. Sandstone running-bond brick used only in the hub. |
| `hub_wall.png` | Hub sandstone brick wall | 64×64 | tiled in a 44px band | Must be **seamless**. Tiled around the hub perimeter as the wall frame. |
| `hub_lantern.png` | Hub wall lantern | 64×64 / frame | 64×64 | **Sprite sheet**, 8 frames — animated flame with a baked-in glow halo. |
| `hub_pillar.png` | Hub sandstone column | 64×144 | ~72×152 | Decoration: columns at the hub corners and mid-walls. |
| `hub_urn.png` | Hub urn | 48×56 | ~44×52 | Decoration placed beside the columns. |
| `hub_banner.png` | Hub hanging banner | 48×96 | 48×96 | Decoration: hangs from the hub's top wall. |
| `attack.png`  | Staff-tip magic arc | 64×64 / frame | 64×64             | **Sprite sheet** (see below). Contains only charge/glow and arc VFX; the staff itself comes from the character sprite. |
| `spell_01_projectile.png` … `spell_17_projectile.png` | Spell projectile animations | 64×64 / frame | varies by spell radius | Per-spell active projectile/effect sprite sheets. |
| `spell_01_enemy.png` … `spell_17_enemy.png` | Spell enemy-hit impacts | 64×64 / frame | varies by spell radius | Played when a non-piercing spell ends by hitting an enemy. |
| `spell_01_wall.png` … `spell_17_wall.png` | Spell wall/pit impacts | 64×64 / frame | varies by spell radius | Played when a spell hits room bounds, pits, or expires. |
| `item.png`    | Pickup/usable     | —             | 28×28             | Optional; no file shipped yet, uses the procedural gold diamond. |

Sprites are scaled up in-game with **nearest-neighbour** filtering, so pixel art
stays crisp/chunky. Authoring at roughly half the draw size (the "Authored size"
column) keeps the scale factor near an integer; you can author larger if you
want more detail — any size works, it'll be scaled to the draw size.

The **Background** kind (the full-window backdrop) is sized to the window and is
always generated, so it has no file mapping.

### Animated sprite sheets (`attack.png`, `dash.png`, `run_portal.png`, `spell_*.png`)

`attack.png`, `dash.png`, `run_portal.png`, and the spell sheets are
**animations**, stored as horizontal sprite sheets: square frames laid
out left-to-right in a single row.

```
[frame0][frame1][frame2] ...   width = frameCount × frameSize, height = frameSize
```

The convention is simple and code-free to extend:

* **Frame size = the sheet's height.** Keep every frame square.
* **Frame count = sheet width ÷ height.** Add or remove frames just by making the
  PNG wider or narrower — no code change.
* Frames play in order across the action's duration. The current `attack.png`
  sheet is 6 frames of 64×64; the spell sheets are 6 frames of 64×64; the dash
  sheets are 5 frames of 24×24; `run_portal.png` is 8 frames of 48×48; and
  `hub_lantern.png` is 8 frames of 64×64 (each hub lantern plays it with a
  per-lantern frame offset so they flicker out of sync).

The same idea works for any future animation: keep the frames square, in one row,
and the renderer can step through them. `attack.png` is the worked example.

### Regenerating the placeholder sprites

The current placeholders were generated by
`tools/gen_placeholder_sprites.ps1`. Re-run it to tweak or rebuild them:

```
pwsh tools/gen_placeholder_sprites.ps1
```

That script is only a convenience for the stand-in art — real art should just be
dropped in as PNGs, overwriting these files.

The hub sandstone tileset (`hub_floor.png`, `hub_wall.png`, `hub_lantern.png`,
`hub_pillar.png`, `hub_urn.png`, `hub_banner.png`) is generated by
`tools/gen_hub_tiles.py` (pure Python standard library, no dependencies):

```
python tools/gen_hub_tiles.py assets
```

The alternate vessel sprites (`character_2.png` … `character_6.png`) are
generated by `tools/gen_characters.py`. The initial `player.png` wizard is
intentionally **not** regenerated by it:

```
python tools/gen_characters.py assets
```

The teleport/dash sheets (`dash.png` … `dash_6.png`) — one per playable vessel,
each a five-frame blink (dematerialize → arcane rift → rematerialize) themed to
that vessel's accent colour — are generated by `tools/gen_dash_sprites.py`.
Vessels 2–6 dissolve their own archetype silhouette (pulled from
`gen_characters.py`); `dash.png` keeps the wizard silhouette unchanged:

```
python tools/gen_dash_sprites.py assets
```

> **TODO — per-vessel attacks.** Every vessel currently shares the one
> `attack.png` staff-arc animation, which only fits the Astral Wizard. The new
> archetypes need their own attack art/behaviour (e.g. the Dust Gunslinger
> should fire a revolver shot, not swing a staff). The combat/`SpellCaster`
> hooks for that are not built yet — when they are, add `attack_2.png` …
> `attack_6.png` (or equivalent) and wire them up per character.

### Adding a brand-new texture kind (programmers)

1. Add a value to `enum class AssetKind` in `include/PlaceholderTextures.hpp`.
2. Map it to a filename in `assetFileName()` and to a procedural fallback in
   `makeProcedural()` (both in `src/PlaceholderTextures.cpp`).
3. Call `m_textures->defaultFor(AssetKind::YourKind)` where you render, and pass
   the texture down (see how `Floor`/`Enemy` flow into `RoomSystem::render` /
   `EnemySystem::render` from `Game::render`).
