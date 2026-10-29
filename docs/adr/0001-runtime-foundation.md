# ADR 0001 — Runtime foundation: upstream Cocos2d-x vs. a maintained fork

- **Status:** Proposed — awaiting project owner decision
- **Date:** 2026-10-29
- **Milestone:** 0

## Context

The brief asks for Cocos2d-x as the runtime/rendering foundation, on a native
macOS ARM64 (Apple silicon) workflow with C++20, CMake and Ninja, and later
Windows and Linux CI. It also says: *if Cocos2d-x cannot be reliably
integrated, report the blocker and offer a concrete resolution before
introducing a substitute.* This ADR is that report.

## What was verified (not assumed)

All of this was checked on 2026-10-29 in a Linux x86_64 cloud workspace
(Ubuntu 24.04, GCC 13.3, Clang 18, CMake 3.28, Ninja 1.11). No Mac was
available, so nothing here was run on macOS.

### Upstream Cocos2d-x (`github.com/cocos2d/cocos2d-x`)

| Check | Finding |
|---|---|
| Latest release | `cocos2d-x-4.0` (December 2019). No release since. |
| Activity | Last commit on the default branch: 2024-11-20 ("fix typo"). Engine development moved to Cocos Creator. |
| License | MIT (plus per-component licenses in `licenses/`). |
| C++ standard | Hard-coded `CMAKE_CXX_STANDARD 11`. |
| Build | CMake 3.6+, but core third-party libraries come **prebuilt** from `cocos2d-x-3rd-party-libs-bin` (tag `metal-support-22`). |
| **macOS arm64** | Every prebuilt macOS library is `x86_64` (some also `i386`); **none contain `arm64`**. Inspected: box2d, bullet, chipmunk, curl, freetype, glfw3, glsl-optimizer, jpeg, openssl, png, libuv, webp, websockets, zlib. A native Apple-silicon link is therefore impossible without rebuilding all of them from source. |
| Linux build (real attempt) | Configure succeeded. Compiling the `cocos2d` library with GCC 13 failed in 2 files (missing `<cstdint>` / `<memory>` includes). After patching those, linking `HelloCpp` failed: the prebuilt `libchipmunk.a` references `__powf_finite` / `__expf_finite`, symbols removed in glibc 2.31. So the prebuilt Linux binaries do not link on current Linux distributions either. |

**Conclusion:** upstream Cocos2d-x 4.0 cannot be integrated reliably on the
primary platform (Apple silicon) or on a current Linux, without us taking over
maintenance of its whole third-party binary set and patching an unmaintained
engine. That is the blocker.

### Axmol Engine (`github.com/axmolengine/axmol`)

Axmol is the actively maintained community fork of Cocos2d-x 4.0 (forked
November 2019; same scene graph / node / action / sprite architecture; the
C++ namespace is `ax` instead of `cocos2d`).

| Check | Finding |
|---|---|
| Latest stable | `v2.11.5` — commit `6c0dce6db4660feafe29dabb1ab3506ee76b0931` (2026-09-25). `v3` exists only as alphas. |
| Activity | Default branch `dev` had a commit on 2026-10-28. |
| License | MIT. Notable bundled components: OpenAL Soft (**LGPL-2.1**), OpenSSL (Apache-2.0), FreeType (FreeType License), ANGLE (BSD), curl (MIT-style). |
| C++ standard | C++20 (CMake reports "Building axmol with c++20"). |
| Build | CMake 3.22+, Ninja supported. Third-party libraries are built from source, plus a few pinned downloads (shader compiler `axslcc`, `1kdist` prebuilts). |
| Platforms | Windows, Linux, macOS (x64 **and arm64**), iOS, Android, WASM per its docs. |
| Dear ImGui | Ships an ImGui extension (ImGui 1.92.1, docking branch) that renders inside the engine. |
| Linux build (real attempt) | See "Probe build results" below. |

## Options

**A. Axmol v2.11.5, pinned to commit `6c0dce6` (recommended).**
Keeps the Cocos2d-x architecture and API style the brief wants, is MIT,
C++20, CMake-native, supports Apple silicon, and is maintained. Cost: it is a
fork, so docs/tutorials written for Cocos2d-x need small renames (`cocos2d::`
→ `ax::`); the LGPL OpenAL Soft dependency must be dynamically linked (or
replaced) in shipped games to keep obligations simple.

**B. Upstream Cocos2d-x 4.0 + our own rebuilt dependencies.**
Rebuild ~15 third-party libraries for arm64/x86_64/Windows/Linux, patch the
engine for modern compilers, stay on C++11 headers. Large ongoing maintenance
for a beginner-led project, on an engine nobody else maintains.

**C. Upstream Cocos2d-x 4.0 under Rosetta (x86_64 on Apple silicon).**
Might work on macOS but is not native, does not fix Linux, and conflicts
with the brief's "native macOS ARM64" priority. Not recommended.

## Decision

Pending. Milestone 0 deliberately ships with **no runtime backend linked**
(`LANTERN_RUNTIME_BACKEND=none`). Everything built so far (core library,
editor entry point, tests) is backend-independent, so no work is lost either
way. Milestone 1 starts by wiring in the chosen backend.

## Consequences (if Option A is accepted)

- Backend is consumed as a pinned Git dependency, never copied into our source tree.
- Public Lantern headers never include `axmol` headers; backend code lives in `engine/platform` / `engine/renderer` implementation files.
- `THIRD_PARTY_NOTICES.md` is extended with Axmol and its bundled licenses before any build is distributed.
- macOS arm64 must be verified on the owner's Mac in Milestone 1; Windows and Linux via CI.
