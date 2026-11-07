# ADR 0014: Lantern Engine becomes Rolo Engine

- **Status:** Accepted
- **Date:** 2026-11-07

## Context

The engine was called Lantern Engine up to 0.9.1-alpha. Several other 2D
engines and frameworks already use the name "Lantern", so searches for it
find other projects, and the name could be confused with theirs. The
flame-and-lantern theme also suggested one kind of game (a cosy top-down
RPG), while Rolo is meant for 2D games of every kind, and later perhaps 3D.

The project owner chose **Rolo Engine**, with a new mascot: Rolo, a round,
smiling orange with big eyes and a green sprout, on a red tile in a black
rounded border (`docs/images/rolo-logo.svg`).

## Decision

Rename everything that names the engine:
- the C++ namespace `lantern` → `rolo`, headers `include/lantern/` → `include/rolo/`;
- programs and targets `lantern_editor`, `lantern_player`, `lantern_tests`
  → `rolo_*`; CMake options and functions `LANTERN_*` / `lantern_*` → `ROLO_*` / `rolo_*`;
- the UI toolkit "Lantern UI" → "Rolo UI", and the flame icon → `Icon::Rolo`;
- the start scene: Rolo's face, drawn with Shape components, replaces the lantern;
- file contents: format and built-in component names `lantern.*` → `rolo.*`
  (`rolo.scene`, `rolo.Transform`), and the project file `project.lantern`
  → `project.rolo`;
- the package `Lantern-<version>-…` → `Rolo-<version>-…`, the README, the
  docs and the logo.

### Projects made with Lantern keep opening

Changing names written into files would otherwise break every existing
project. Instead of bumping every format's version, the names are treated
as aliases (`rolo/core/Names.hpp`):
- `isFormatName` accepts a file whose format is the old `lantern.` name of
  the expected one;
- `upgradeLegacyName` maps `lantern.X` to `rolo.X`; the component registry
  uses it to find built-in components saved under their old IDs;
- `Project::open` reads `project.lantern` when there is no `project.rolo`;
  saving writes `project.rolo` and removes the old file.

The data inside the files is unchanged, so no upgrade step is needed: the
next save simply writes the new names. `tests/scene/test_legacy_names.cpp`
opens a project exactly as Lantern 0.9.0 wrote it.

### What keeps its name

- **The Lantern Keeper**, the demo game: its story, the "Lantern" object,
  the quest "The Dark Lantern" and its `lantern_*` content IDs are the
  game's own, not the engine's.
- File extensions (`.lscene`, `.lanim`, `.ltileset`, `.lquest`, `.ldialogue`, `.lvars`): they are
  short and do not spell out the old name, and changing them would rename
  every file in every project for no gain.
- The changelog and older ADRs were updated to say "Rolo" throughout, so
  they read as one history; this ADR records the change.

## Consequences

- Old projects open and are upgraded on save; Lantern 0.9.x cannot open a
  project once Rolo has saved it (the same as any format upgrade).
- Game modules written for Lantern must be updated: `#include "lantern/…"`
  → `"rolo/…"`, `lantern::` → `rolo::`, `LANTERN_GAME_MODULE` → `ROLO_GAME_MODULE`.
  Lua scripts need no changes.
- The GitHub repository is renamed to `Rolo`; GitHub redirects the old URL.
