# ADR 0002 — Runtime module and how Axmol is integrated

- **Status:** Accepted
- **Date:** 2026-10-29
- **Milestone:** 1

## Context

ADR 0001 chose Axmol v2.11.5 as the rendering/runtime backend. Milestone 1
needs a window, a main loop, clean start-up/shut-down and logging, while the
brief requires that game code talks to Lantern, not to the backend.

## Decisions

### 1. Module layout

| Target | Depends on | Purpose |
|---|---|---|
| `lantern_core` | C++ standard library only | `Engine`, `Logger`, `Status`, version |
| `lantern_runtime` | `lantern_core` (public), Axmol (**private**) | `lantern::Application`: window, main loop, per-frame callback, screenshots |
| `lantern_player_cli` | nothing | `lantern_player` command-line parsing (unit-tested) |
| `lantern_player` | `lantern_runtime`, `lantern_player_cli` | the game runtime executable (`.app` bundle on macOS) |
| `lantern_editor` | `lantern_runtime`, `lantern_editor_core` | the editor (window since Milestone 2, see ADR 0003) |

`lantern/runtime/Application.hpp` contains no Axmol types: implementation
state lives behind a pointer (`Application::Impl`). Linking Axmol `PRIVATE`
means a target that uses `lantern_runtime` cannot even `#include` Axmol
headers by accident.

### 2. Lifecycle

`Engine::initialize()` → `Application::run()` (opens the window, blocks until
it closes) → `Engine::shutdown()`. `run()` refuses to start if the engine is
not running, if it was already called, or if another `Application` is
running (Axmol's `Director` is a process-wide singleton; the guard turns
misuse into a clear error instead of a crash).

### 3. Logging

Axmol's log output is redirected into the engine's `Logger` (category
`axmol`) for the duration of `run()`, so all diagnostics flow through one
place — the terminal today, the editor console in Milestone 2.

### 4. How Axmol is obtained (no Axmol setup script)

`cmake/LanternAxmol.cmake`:

1. Clones Axmol tag `v2.11.5` into `external/axmol` and **verifies** the
   commit is `6c0dce6…`; a mismatch stops the build with instructions.
2. Downloads Axmol's shader compiler `axslcc` 1.14.0 for the host OS with a
   **SHA-256 check**, into the folder where Axmol expects it.
3. Turns off Axmol features not needed yet (3D, physics, audio, extensions)
   to shorten builds and keep LGPL OpenAL Soft out for now. (Axmol's debug
   console stays compiled in: disabling it breaks Axmol's own Debug build.
   It is never started by Lantern.)
4. Adds Axmol's core with `add_subdirectory(... SYSTEM)` so our strict
   warnings do not apply to its headers.

We deliberately do **not** run Axmol's `setup.ps1`: it edits shell profiles
and, on macOS, refuses to continue without the full Xcode app. Axmol's CMake
still requires **PowerShell 7 (`pwsh`)** for its prebuilt-library
downloader, so `pwsh` is a documented prerequisite.

The `external/` folder is shared by all build presets and ignored by git.

### 5. Presets

- `dev` / `release`: with Axmol (default).
- `core` / `asan`: no backend — builds in seconds, used for quick checks and
  sanitizer runs of the core library.

## Consequences

- First configure downloads ~130 MB and the first build compiles Axmol
  (minutes); later builds are incremental.
- macOS with Command Line Tools only (no full Xcode) is confirmed working:
  33/33 tests on the owner's MacBook Pro (macOS 15.4.1, CLT 16.4), 2026-10-29.
- Re-enabling Axmol features later (audio, physics) is a one-line change in
  `LanternAxmol.cmake` plus a `THIRD_PARTY_NOTICES.md` update. Milestone 2
  re-enabled the ImGui extension this way (ADR 0003).
