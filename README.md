# Lantern Engine

A C++-first 2D game engine and standalone editor for indie games — top-down
RPGs, pixel-art adventures, tile-based and story-driven games.

> **Status: Milestone 0 (foundation).** The build system, core library, editor
> entry point and test suite exist and pass. There is no window, renderer or
> editor UI yet.

## Quick start

```sh
cmake --workflow --preset dev
./build/dev/bin/lantern_editor
```

Full instructions per platform: [docs/building.md](docs/building.md).

## Repository layout

```
engine/core/     Engine lifecycle, logging, version (no third-party deps)
editor/          Standalone editor application (UI arrives in Milestone 2)
tests/           Catch2 unit tests + editor smoke tests
cmake/           Shared CMake helpers
docs/            Build guide and architecture decision records (ADRs)
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

Lantern Engine runs on [Axmol](https://github.com/axmolengine/axmol), the
maintained fork of Cocos2d-x (wired in from Milestone 1). Upstream Cocos2d-x
4.0 was evaluated first but cannot be built natively on Apple silicon or
linked on current Linux. Details and evidence:
[docs/adr/0001-runtime-foundation.md](docs/adr/0001-runtime-foundation.md).

## License

Lantern Engine is released under the [MIT License](LICENSE).
Copyright (c) 2026 Oscar Hird.

Third-party components keep their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
