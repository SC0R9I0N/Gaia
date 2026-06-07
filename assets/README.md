# Assets

Drop game resources here — textures, audio, fonts, level data, etc.

This folder is copied next to the built executable after every build (see the
`POST_BUILD` step in the root `CMakeLists.txt`), so load files at runtime using a
relative path such as `assets/player.png`.

## Placeholders

The game does not need any files in here to run. Until real art exists,
`PlaceholderTextures` (in `src/PlaceholderTextures.cpp`) generates simple
stand-in textures at runtime for each kind of object:

- **Character** — a teal square with a facing arrow
- **Item** — a gold diamond
- **Background** — a dark grid

To use real art instead, drop a `.png` here and load it in
`PlaceholderTextures::defaultFor()` (or alongside it), then return that texture
for the matching `AssetKind`.
