# Contributor and AI-assistant rules

Read this before changing anything. It applies equally to human contributors
and AI assistants.

## Working rules

1. Inspect the repository before editing; preserve working code.
2. Small, reviewable changes. Every change leaves `cmake --workflow --preset dev` passing.
3. Never report a build or test as passing unless it was actually run. Say where it ran (OS, compiler).
4. No mockups or hard-coded demos presented as finished features; no TODOs reported as done.
5. Every new dependency needs a reason, a pinned version, and an entry in `THIRD_PARTY_NOTICES.md`.
6. Bugs: reproduce, find the root cause, fix, add a regression test.
7. Update docs when public APIs, build steps or file formats change.
8. Architectural changes need an ADR in `docs/adr/`.

## Architecture rules

- Dependency direction: `editor` → `engine`; never the reverse. Tests may use anything.
- Public engine headers (`engine/*/include/lantern/...`) must not include backend (Axmol) headers.
- The editor interface is built with Lantern UI (`engine/ui`, `lantern::ui`). Do not add Dear ImGui or any other third-party UI library (ADR 0004).
- `engine/ui` must not depend on Axmol, GLFW or any graphics API: it turns widgets into a list of triangles, and `engine/runtime/src/UiHost.*` draws them. Keep it unit-testable without a window.
- `editor/model/` must not include UI or runtime/backend headers: it is the editor logic, unit-tested without a window.
- Editor UI code (`editor/application`, `editor/panels`) may use `lantern/ui/*` and `lantern/runtime/*.hpp`, never Axmol headers.
- Editor operations that change scene or project data must go through the undo/redo command system: panels call the editor's `execute()` with a `Command` (`editor/model/Commands.hpp`), never `Scene` setters directly.
- Scenes are `lantern::Scene` data (`engine/scene`); Axmol nodes are created only by `engine/runtime/src/SceneRenderer.*`. New component types describe their properties with `ComponentTypeBuilder` so the Inspector, files and undo handle them automatically.
- File formats (`docs/formats/`) are versioned: changing one means bumping its version, adding an upgrade step and a test with an old file.
- No global mutable state. Pass `Engine&` / `Logger&` explicitly.
- Ownership: RAII and `std::unique_ptr` by default; raw pointers only as non-owning observers.
- Errors: return `lantern::Status` (or a richer result type later) with a user-actionable message. No exceptions across module boundaries.
- Stable IDs, not names or addresses, for anything saved to disk.
- Saved files use project-relative paths and an explicit format version.

## Code style

- C++20, `.clang-format` in the root. 4-space indent.
- Namespace `lantern`. Types `PascalCase`, functions `camelCase`, members `m_name`, constants `kName`.
- Headers in `include/lantern/<module>/`, sources in `src/`.
- Write comments for a reader who is learning engine programming: explain *why*.

## Commands

```sh
cmake --workflow --preset dev   # configure + build + test (window tests need a display;
                                # on headless Linux: xvfb-run -a ctest --preset dev)
cmake --workflow --preset core  # fast: no Axmol, no window
cmake --build --preset asan && ctest --preset asan   # memory-error check
```
