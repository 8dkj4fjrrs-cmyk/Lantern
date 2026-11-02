# Lantern Engine

A C++-first 2D game engine and standalone editor for indie games — top-down
RPGs, pixel-art adventures, tile-based and story-driven games.

> **Status: Milestone 7 (a complete demo game).** Play **The Lantern
> Keeper**, a small top-down RPG made with Lantern:
> `lantern_player --project examples/lantern_keeper/project`
> ([how it was made](docs/making-the-lantern-keeper.md)).
>
> `lantern_editor` creates and opens
> projects, imports assets with permanent IDs, and edits scenes with
> sprites, shapes, text, cameras, colliders, tilemaps and sprite-sheet
> animations (made in its Animation panel), all with undo, saved as
> versioned JSON. Gameplay is written in C++ as **behaviours** in
> game modules; story content (game variables, dialogues, quests, and
> triggers in scenes) is data, edited in the editor and checked as you go.
> Play mode runs the game in the Scene view on a copy of the scene;
> `lantern_player` plays a project with input, collision, scene changes,
> dialogue and save games.

## Quick start

```sh
brew install cmake ninja git && brew install powershell   # macOS, once
cmake --workflow --preset dev
./build/dev/bin/lantern_editor/lantern_editor.app/Contents/MacOS/lantern_editor
```

Full instructions per platform: [docs/building.md](docs/building.md).
Using the editor: [docs/editor.md](docs/editor.md).
Writing gameplay in C++: [docs/gameplay.md](docs/gameplay.md).
What comes next: [docs/roadmap.md](docs/roadmap.md).

## Repository layout

```
engine/core/     Engine lifecycle, logging, version (no third-party deps)
engine/scene/    Scenes, entities, components, scene and project files (no backend)
engine/assets/   Asset IDs (.meta files), import, image checks, tilesets (no backend)
engine/content/  RPG content: variables, dialogue, quests, conditions, actions, saves (no backend)
engine/gameplay/ Behaviours, game modules, input, collision queries, scene changes (no backend)
engine/ui/       Lantern UI: the editor's own interface toolkit (widgets, docking, text)
engine/runtime/  Window and main loop (lantern::Application); wraps Axmol privately
player/          lantern_player, the game runtime application
examples/        The Lantern Keeper (the demo RPG) and the sample game: game modules and projects
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
