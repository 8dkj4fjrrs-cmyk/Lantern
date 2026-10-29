# Building Lantern

Milestone 0 builds three things: the `lantern_core` library, the
`lantern_editor` executable (console-only for now), and the `lantern_tests`
test runner. No game-engine backend is linked yet, so no window opens.

## macOS (Apple silicon) — primary platform

Verified 2026-10-29 on the project owner's Apple silicon MacBook Pro
(Homebrew 7.0.11, CMake 4.4.5, Ninja 1.13.2, Apple Clang): 18/18 tests passed
and the editor ran.

### 1. One-time setup

```sh
# Apple's compiler and SDK (skip if `clang --version` already works)
xcode-select --install

# Homebrew: https://brew.sh — then:
brew install cmake ninja git
```

Check versions — you need CMake **3.25 or newer** for the presets:

```sh
cmake --version
ninja --version
clang++ --version
```

### 2. Configure, build, test

From the repository root:

```sh
cmake --preset dev          # configure (Debug, Ninja, warnings as errors)
cmake --build --preset dev  # compile
ctest --preset dev          # run all tests
```

Or all three in one command:

```sh
cmake --workflow --preset dev
```

The first configure downloads Catch2 v3.16.1 (test framework) unless it is
already installed (`brew install catch2` makes this offline).

### 3. Run the editor

```sh
./build/dev/bin/lantern_editor
./build/dev/bin/lantern_editor --version
./build/dev/bin/lantern_editor --help
```

Expected output of the first command:

```
[INFO] engine: Starting 'Lantern Editor' with Lantern 0.0.1 (backend: none)
[INFO] editor: The editor window is not implemented yet (planned for Milestone 2).
[INFO] engine: Shut down 'Lantern Editor'
```

### Other presets

| Preset | Purpose |
|---|---|
| `dev` | Debug build, warnings are errors. Use this day to day. |
| `release` | Optimised build. |
| `asan` | Debug + AddressSanitizer/UBSan — finds memory bugs. Use when something crashes mysteriously. |

Each preset builds into its own folder under `build/`, so they never interfere.
To start completely fresh, delete the `build/` folder.

## Linux (Ubuntu 24.04)

```sh
sudo apt install build-essential cmake ninja-build git
cmake --workflow --preset dev
```

Ubuntu 24.04 ships CMake 3.28, which is new enough.

## Windows

Not yet verified. Planned via CI (`.github/workflows/ci.yml`) once the
repository is on GitHub. Expected commands, from a "Developer PowerShell for
VS 2022":

```powershell
cmake --workflow --preset dev
```
