# Assets

Drop game resources here — textures, audio, fonts, level data, etc.

This folder is copied next to the built executable after every build (see the
`POST_BUILD` step in the root `CMakeLists.txt`), so load files at runtime using a
relative path such as `assets/player.png`.
