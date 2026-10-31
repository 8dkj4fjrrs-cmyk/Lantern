# Lantern Engine

A C++-first 2D game engine and standalone editor for indie games — top-down
RPGs, pixel-art adventures, tile-based and story-driven games.

> **Status: Milestone 4 (sprites and tilemaps).** `lantern_editor` creates
> and opens projects; imports images, sounds and fonts with permanent asset
> IDs; and edits scenes with sprites, shapes, cameras and tilemaps (layers,
> brush, eraser, fill, collision), a free Scene view camera, and move,
> rotate, scale and resize tools, all with undo, saved as versioned JSON.
> `lantern_player` plays a project or a scene file. Gameplay code and Play
> mode arrive in Milestone 5.

## Quick start

```sh
brew install cmake ninja git && brew install powershell   # macOS, once
cmake --workflow --preset dev
./build/dev/bin/lantern_editor/lantern_editor.app/Contents/MacOS/lantern_editor
```

Full instructions per platform: [docs/building.md](docs/building.md).
Using the editor: [docs/editor.md](docs/editor.md).

## Repository layout

```
engine/core/     Engine lifecycle, logging, version (no third-party deps)
engine/scene/    Scenes, entities, components, scene and project files (no backend)
engine/assets/   Asset IDs (.meta files), import, image checks, tilesets (no backend)
engine/ui/       Lantern UI: the editor's own interface toolkit (widgets, docking, text)
engine/runtime/  Window and main loop (lantern::Application); wraps Axmol privately
player/          lantern_player, the game runtime application
editor/model/    Editor logic without UI code (console, preferences, state, undo commands)
editor/panels/   Editor panels (built with Lantern UI)
editor/application/  Editor window, menus, docking layout
tests/           Catch2 unit tests + window smoke tests
cmake/           Shared CMake helpers, incl. the pinned Axmol download
docs/            Build guide and architecture decision records (ADRs)
external/        Downloaded Axmol + tools (created by CMake, not in git)
```

Further modules (renderer, scene, serialization, dialogue, quests, …) are
added by the milestone that needs them, not as empty placeholders.

## Architecture rules

- The engine never depends on the editor.
- Public engine headers never expose the runtime backend's types.
- No global mutable state: loggers and engines are ordinary objects.
- Every milestone leaves the project building with all tests passing.

See [AGENTS.md](AGENTS.md) for the full contributor rules.

## Runtime backend

Lantern Engine runs on [Axmol](https://github.com/axmolengine/axmol) v2.11.5,
the maintained fork of Cocos2d-x, pinned to an exact commit. Upstream Cocos2d-x
4.0 was evaluated first but cannot be built natively on Apple silicon or
linked on current Linux. Details and evidence:
[docs/adr/0001-runtime-foundation.md](docs/adr/0001-runtime-foundation.md).

## License

Lantern Engine is released under the [MIT License](LICENSE).
Copyright (c) 2026 Oscar Hird.

Third-party components keep their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
