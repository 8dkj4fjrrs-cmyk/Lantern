# Changelog

All notable changes to Lantern Engine. Versions follow the milestones in
[docs/roadmap.md](docs/roadmap.md); design reasons are in [docs/adr](docs/adr).

## Unreleased

- **Lua scripting.** Gameplay code without a compiler: write a `.lua` file,
  add a **Script** component, press Play. A downloaded Lantern runs scripts,
  and exported games carry them.
  - Scripts get `start(self)` and `update(self, dt)`, and can use:
    - objects: move with collisions, animate, destroy, set text...;
    - `game.*`: find objects, variables, events, the content notation,
      scene changes, saves;
    - `input.*`, `print` (to the Console) and `require` (modules).
  - Sandboxed: no files, programs or native code.
  - A script error names the file and line and stops only that script. A
    script stuck in a loop, or using too much memory, is stopped.
  - **New Script** (Assets panel and Game menu), and the script's Inspector
    page: syntax check, preview, **Open in Code Editor**. **Check Content**
    and exporting check scripts' syntax.
  - The Top-Down Starter has Lua fireflies as an example.
  - Guide: [docs/scripting.md](docs/scripting.md); design: ADR 0013. Lua
    5.4.9 (MIT) is kept in `third_party/lua`.
- **macOS and Windows build in CI.** The first runs on GitHub found and fixed:
  - Apple's standard library lacks decimal `std::from_chars`;
  - Apple Clang 15 cannot construct aggregates from parentheses;
  - MSVC warnings (`getenv`, unreachable code);
  - Axmol's Windows app setup: a CMake link-style clash (Axmol patch 0002)
    and the `WinMain` entry point;
  - the Axmol patch's line endings on Windows;
  - a missing `_sv` literal;
  - the export test on macOS (games are `.app` bundles).
- **No more yasio, curl, llhttp or websocket-parser in games.** Lantern now
  builds Axmol without its network module and remote console, which Lantern
  never used. The few yasio containers and helpers that Axmol's core still
  needs are replaced by Lantern's own code (`engine/runtime/axmol_compat`).
  Exported games no longer carry yasio's non-standard "Anti 996" license
  terms. See ADR 0012.
- Axmol is now patched at configure time from `cmake/axmol-patches/`. An
  existing `external/axmol` folder is patched in place the next time you
  configure.

## 0.8.0-alpha — Public alpha (Milestone 8)

The first version meant for people other than its authors. See
[known limitations](docs/known-limitations.md) before relying on it.

- **New Project wizard** with templates:
  - Empty Project;
  - Top-Down Starter: a small game that plays at once, with no C++;
  - the example game The Lantern Keeper.

  Also `lantern_editor --new-project FOLDER --template topdown`.
- **Exporting games**:
  - **Build** (or **File > Export Game…**) checks the project and makes a
    copy of the game that runs without the editor, with the license texts it
    must carry. Also `lantern_editor --project P --export OUT`.
  - The player plays a game bundled beside it when started with no arguments.
- **Built-in gameplay behaviours**, so games need no C++ to start:
  - Top-Down Controller: walking, with walk and idle clips;
  - Scene Door: arrives at a named object in the next scene;
  - Camera Bounds.

  `Game::changeScene(scene, arriveAt)`.
- **Packaging**: install rules and `lantern_package`, which makes the
  downloadable Lantern folder and zip. Release preset and workflows.
- **Licenses**: `licenses/` holds the full license texts of everything
  inside a game, fetched from each project at the versions used, and every
  export carries it. THIRD_PARTY_NOTICES.md now also lists the header-only
  libraries compiled into games. It also records the review of yasio's
  "Anti 996" terms.
- **Onboarding**:
  - a welcome screen on first start;
  - a getting-started guide, an exporting guide and release instructions;
  - clearer messages when a project, a template or the runtime is missing.
- The editor finds its templates, examples and the game runtime in an
  installed copy or in a build folder (`LanternResources`).

## 0.7.0 — The Lantern Keeper (Milestone 7)

- The Lantern Keeper, a complete demo RPG built from a fresh project, with a
  step-by-step guide to rebuilding it. An automated test plays it to the end.
- Make Start Scene in the editor (undoable).

## 0.6.5 — Flipbook animation (Milestone 6.5)

- Animation sets (`.lanim`), the Animator component, the Animation panel,
  and `Game::play` / `isAnimationFinished`.
- Fixes: the asset database dropped animation data on refresh; Enter in a
  dialog's text field now presses its main button.

## 0.6.0 — RPG content (Milestone 6)

- Variables, dialogues with choices and branches, quests, typed conditions
  and actions, triggers, events, the dialogue box, save games, content
  checks, and form-based content editing in the Inspector.

## 0.5.0 — Gameplay (Milestone 5)

- C++ behaviours in game modules, play mode in the editor, input,
  collision queries, scene changes, and the sample game.

## 0.4.0 — Assets and tilemaps (Milestone 4)

- Asset IDs and import, sprites, the Scene view camera, tilesets, tilemaps
  and painted collision.

## Earlier milestones

- 0.3: scenes and serialization, stable IDs, undo/redo (Milestone 3).
- 0.2: the editor shell with docking panels, built with Lantern UI (Milestone 2).
- 0.1: the Axmol runtime, the window and start-up (Milestones 0 and 1).
