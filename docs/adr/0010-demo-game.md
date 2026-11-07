# ADR 0010: The demo game, The Lantern Keeper (Milestone 7)

- **Status:** Accepted
- **Date:** 2026-11-02
- **Milestone:** 7

## Context

Milestone 7 asks for a small, complete top-down RPG that demonstrates the
engine, with a tilemap, player movement, collisions, interactions,
dialogue, quests and saving. It must be built from a fresh project, and the
documentation must show how another developer can reproduce it. The
existing `examples/sample_game` is a test bed: two rooms and gems, no story
or ending, and its project files were generated rather than made from a
project the editor created.

## Decisions

### 1. A new example with its own module; the sample stays the test bed

The game lives in `examples/lantern_keeper`: its game module `keeper`, its
project, and its tools. The behaviour ids are `keeper.*`. Engine tests keep
using the small sample, whose shape they rely on. Both modules are linked
into the editor and the player (`ROLO_BUILD_SAMPLE_GAME`), which also
proves that several game modules work side by side.

### 2. As much as possible is content; C++ only where content cannot reach

The story is entirely triggers, dialogues and quests:
- talking, reading and the save point;
- picking up flames;
- the gate opening for good, through an event;
- Biscuit going home, the night lifting, and the ending.

The module has four behaviours that a designer could not express as
content:
- **PlayerController**: input, movement and choosing animation clips.
- **SceneDoor** and **SpawnPoint**: arriving where you came in. The arrival
  point travels in a `text` content variable, so it is saved and checked
  like any other variable.
- **CameraLimits**: keeps the camera inside the map.

This is the balance the engine aims for, and the guide teaches it.

### 3. Built from a fresh project by a script that writes what the editor writes

Driving the editor by hand for hundreds of placements is not reproducible.
Instead:
1. The shipped project starts as `rolo_editor --new-project`.
2. `tools/make_art.py` draws the art.
3. `tools/build_project.py` adds the rest, writing the same files and
   formats the editor writes, with fixed IDs so the output never changes
   between runs.

The guide (`docs/making-the-lantern-keeper.md`) gives the same steps in the
editor. The result is an ordinary project, opened and changed in the editor
like any other.

### 4. The game is tested as a game

`tests/gameplay/test_lantern_keeper.cpp` loads the real project with the
real module and plays the whole story through the game's rules: walking
into doors and items, pressing E, reading every dialogue, saving and
loading. It also checks that every content reference and animation clip
resolves, and that the conditions and actions written in the guide parse.

Window tests run the real player (keys only: the intro, walking to Maren,
accepting the quest) and the editor's play mode on the project.

### 5. Fixing gaps the game revealed, in the engine where they are general

- The editor gained **Make Start Scene**, an undoable `SetStartScene`
  command. The editor re-reads `project.rolo` when it changes on disk.
- The editor-hostile night overlay is saved switched off and switched on by
  a `sceneStart` trigger.
- Smaller gaps are recorded in the roadmap rather than fixed here (scene
  renaming, creating folders, built-in spawn points and camera limits).

## Consequences

- No engine format changes. Version 0.7.0.
- The game adds little to the repository: the art is a few small PNGs,
  and everything else is text.
- Later engine changes that break the game's story fail the test suite,
  which keeps the demo honest.
