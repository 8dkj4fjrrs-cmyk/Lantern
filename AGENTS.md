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
- Public engine headers (`engine/*/include/lantern/...`) must not include backend (Cocos/Axmol) or ImGui headers.
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
cmake --workflow --preset dev   # configure + build + test
cmake --build --preset asan && ctest --preset asan   # memory-error check
```
