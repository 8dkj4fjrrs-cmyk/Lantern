# Lantern Engine

A C++-first 2D game engine and standalone editor for indie games — top-down
RPGs, pixel-art adventures, tile-based and story-driven games.

> **Status: 0.8.0-alpha, the first public alpha.** Make a project from a
> template, build a small game in the editor without writing code, and
> export it as a program players run without Lantern. It has been built and
> tested on Linux only so far; read the
> **[known limitations](docs/known-limitations.md)** before relying on it.

What is in it:

- **An editor** with:
  - scenes, sprites, tilemaps, colliders, text and cameras;
  - flipbook animation;
  - dialogue, quests and triggers;
  - play mode;
  - full undo.

  It is built with Lantern's own UI toolkit.
- **A New Project wizard**: Empty, a **Top-Down Starter** that plays at once,
  or a copy of the demo game.
- **Exporting**: **Build** checks the project and makes a stand-alone game
  with the license texts it must carry.
- **Gameplay** without code (built-in behaviours, triggers, dialogue, quests,
  saves); **Lua scripts** when you want more, with no compiler needed; and
  C++ behaviours in game modules for those who build Lantern from source.
- **[The Lantern Keeper](examples/lantern_keeper)**, a small finished RPG
  ([how it was made](docs/making-the-lantern-keeper.md)).

## Start here

- **[Getting started](docs/getting-started.md)**: from a new project to a
  game you can share, in half an hour.
- [Editor guide](docs/editor.md) · [Scripting in Lua](docs/scripting.md) ·
  [Gameplay in C++](docs/gameplay.md) ·
  [Exporting](docs/exporting.md) · [File formats](docs/formats/scene.md) ·
  [Roadmap](docs/roadmap.md) · [Changelog](CHANGELOG.md)

## Building from source

```sh
brew install cmake ninja git && brew install powershell   # macOS, once
cmake --workflow --preset dev
./build/dev/bin/lantern_editor/lantern_editor.app/Contents/MacOS/lantern_editor
```

Full instructions per platform: [docs/building.md](docs/building.md).
Using the editor: [docs/editor.md](docs/editor.md).
Writing gameplay in Lua: [docs/scripting.md](docs/scripting.md); in C++: [docs/gameplay.md](docs/gameplay.md).
What comes next: [docs/roadmap.md](docs/roadmap.md).

## Repository layout

```
engine/core/     Engine lifecycle, logging, version (no third-party deps)
engine/scene/    Scenes, entities, components, scene and project files (no backend)
engine/assets/   Asset IDs (.meta files), import, image checks, tilesets (no backend)
engine/content/  RPG content: variables, dialogue, quests, conditions, actions, saves (no backend)
engine/gameplay/ Behaviours, Lua scripts, game modules, input, collision queries, scene changes (no backend)
engine/ui/       Lantern UI: the editor's own interface toolkit (widgets, docking, text)
engine/runtime/  Window and main loop (lantern::Application); wraps Axmol privately
player/          lantern_player, the game runtime application
examples/        The Lantern Keeper (the demo RPG) and the sample game: game modules and projects
templates/       New Project templates (Empty, Top-Down Starter)
licenses/        License texts of everything inside an exported game
tools/release/   Release helpers (collecting license texts)
editor/model/    Editor logic without UI code (console, preferences, state, undo commands)
editor/panels/   Editor panels (built with Lantern UI)
editor/application/  Editor window, menus, docking layout
tests/           Catch2 unit tests + window smoke tests
cmake/           Shared CMake helpers, incl. the pinned Axmol download
docs/            Build guide and architecture decision records (ADRs)
external/        Downloaded Axmol + tools (created by CMake, not in git)
```

Contributing: [CONTRIBUTING.md](CONTRIBUTING.md). Releasing:
[docs/releasing.md](docs/releasing.md).

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
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [licenses/](licenses/README.md).
Games exported with Lantern carry those license texts. Lantern builds Axmol
without its networking libraries, so games contain only permissively
licensed code (ADR 0012).
