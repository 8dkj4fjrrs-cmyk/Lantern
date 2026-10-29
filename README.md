# Lantern Engine

A C++-first 2D game engine and standalone editor for indie games — top-down
RPGs, pixel-art adventures, tile-based and story-driven games.

> **Status: Milestone 1 (runtime foundation).** `lantern_player` opens a
> window and runs the main loop on the Axmol backend. The editor is still a
> console program (its window arrives in Milestone 2).

## Quick start

```sh
brew install cmake ninja git && brew install powershell   # macOS, once
cmake --workflow --preset dev
./build/dev/bin/lantern_player/lantern_player.app/Contents/MacOS/lantern_player
```

Full instructions per platform: [docs/building.md](docs/building.md).

## Repository layout

```
engine/core/     Engine lifecycle, logging, version (no third-party deps)
engine/runtime/  Window and main loop (lantern::Application); wraps Axmol privately
player/          lantern_player, the game runtime application
editor/          Standalone editor application (UI arrives in Milestone 2)
tests/           Catch2 unit tests + editor smoke tests
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
