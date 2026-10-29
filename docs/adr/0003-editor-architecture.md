# ADR 0003 — Editor architecture (Milestone 2)

- **Status:** Accepted
- **Date:** 2026-10-29
- **Milestone:** 2

## Context

The brief asks for a standalone desktop editor with dockable panels, a
persistent layout, a viewport, hierarchy, inspector and console, built with
Dear ImGui initially — while keeping editor logic independent enough of the
UI library that it could be replaced later, and keeping the runtime free of
editor code.

## Decisions

### 1. Three layers

| Layer | Target | Knows about |
|---|---|---|
| Editor logic | `lantern_editor_core` (`editor/model/`) | C++ and `lantern_core` only. Console log store, preferences file, settings folders, editor state, command line. |
| Editor UI | `lantern_editor` (`editor/application/`, `editor/panels/`) | Dear ImGui + `lantern_runtime`'s public API. No Axmol. |
| Runtime | `lantern_runtime` | Axmol + the Axmol ImGui extension, privately. Knows nothing about the editor. |

Everything worth testing without a window lives in the logic layer (unit
tests run in every preset, including the backend-free `core` preset). If
Dear ImGui is ever replaced, only `editor/application` and `editor/panels`
change.

The editor gets Dear ImGui's **headers only** through an interface target
(`lantern_imgui_headers`), so editor code cannot include Axmol headers by
accident. The ImGui library itself is linked through `lantern_runtime`.

### 2. What the runtime offers tools (still no backend types)

`PresentationSettings` (in `lantern/runtime/Application.hpp`):

- `worldDisplay = Offscreen`: the game world is rendered each frame into a
  texture of the game's resolution (1280×720 by default) instead of the window.
- `ui` callback: an immediate-mode UI layer drawn over the window each frame.
  Inside it, `UiFrame` provides:
  - `worldImage(w, h)` — draws the world texture letterboxed into the panel;
  - `sceneOutline()` — a read-only snapshot (`SceneNodeInfo`) of the scene;
  - `framesPerSecond()`, `requestQuit()`.
- `uiSetup` / `uiShutdown` hooks — load and save the layout at the right moments.

Games use the same `Application` with the defaults (world fills the window,
no UI layer).

### 3. Viewport = game view

The world keeps its own fixed resolution; the viewport panel shows it scaled
to fit, with its size and zoom percentage. The world stays in the running
scene (so animations and timers run normally) but is hidden from the window
and only drawn into the texture. Camera controls and zoom come in Milestone 4.

### 4. Hierarchy and inspector are read-only until Milestone 3

They show the **live** running scene (names, kinds, transforms) through
`sceneOutline()`. Ids in the outline are session-only — never saved. Editing
is deliberately left for Milestone 3, where Lantern's own entities with stable
ids and an undo/redo command system arrive, so every edit is undoable from day
one.

### 5. Persistence

- Layout (docking positions, sizes): Dear ImGui's `.ini` text, saved by the
  editor (not by ImGui) to `layout.ini`, a few seconds after a change and on exit.
- Preferences (panel visibility, window size): `editor.prefs`, a versioned
  `key = value` file, saved on exit; unknown keys are preserved.
- Both live in the per-user settings folder (see `docs/editor.md`), written
  atomically (temporary file + rename). `--config-dir` overrides the folder;
  automated tests always use it, so they never touch real settings.

### 6. UI font

The editor uses a common system font (San Francisco on macOS, Segoe UI on
Windows, DejaVu Sans on Linux) loaded at run time; nothing is redistributed.
If none is found it falls back to Dear ImGui's built-in font.

## Consequences

- One extra Axmol extension (ImGui, MIT) is built and linked into
  `lantern_runtime`, so the player also contains it (unused there for now).
- Separate OS windows for panels (ImGui "multi-viewport") are switched off:
  Axmol only supports them with OpenGL, and consistency across platforms
  matters more.
- Axmol's ImGui extension shuts down on the `Director::EVENT_RESET` event,
  while the window still exists; the runtime releases its UI resources there.
