# Building Lantern Engine

A full build produces:

| Program | What it is |
|---|---|
| `lantern_player` | The game runtime: opens a window and runs the main loop. A `.app` bundle on macOS. |
| `lantern_editor` | The editor (console-only until Milestone 2). |
| `lantern_tests`, `lantern_runtime_tests` | Automated tests, run by `ctest`. |

The window and rendering come from [Axmol](https://github.com/axmolengine/axmol)
(see `docs/adr/`). CMake downloads Axmol and its shader compiler automatically
the first time; you only install the tools below.

## macOS (Apple silicon) — primary platform

Milestone 0 (core only) was verified on 2026-10-29 on the project owner's
MacBook Pro (Homebrew 7.0.11, CMake 4.4.5, Ninja 1.13.2, Apple Clang).

### 1. One-time setup

```sh
xcode-select --install            # Apple's compiler (skip if already installed)
brew install cmake ninja git      # build tools (Homebrew: https://brew.sh)
brew install powershell    # needed by Axmol's download helper
```

If that fails, install the official package from
https://aka.ms/powershell-release?tag=stable (the `osx-arm64.pkg` file).
Open a **new** Terminal
window afterwards, then check:

```sh
cmake --version    # 3.25 or newer
pwsh --version     # PowerShell 7.x
```

### 2. Configure, build, test

From the repository root:

```sh
cmake --workflow --preset dev
```

The **first** run downloads Axmol (about 130 MB) and compiles it, which takes
roughly 10–20 minutes. Later builds only recompile what changed.

### 3. Run the player

To see log messages in Terminal, run the program inside the app bundle:

```sh
./build/dev/bin/lantern_player/lantern_player.app/Contents/MacOS/lantern_player
```

Or open it like any Mac app (no log output):

```sh
open build/dev/bin/lantern_player/lantern_player.app
```

A window titled "Lantern Engine" shows a softly pulsing lantern and a
frame-rate overlay. Press **Escape** or close the window to quit.
`--help` lists the options.

### 4. Run the editor

```sh
./build/dev/bin/lantern_editor
```

## Presets

| Preset | Purpose |
|---|---|
| `dev` | Debug build with the Axmol runtime; warnings are errors. Use this day to day. |
| `release` | Optimised build with the Axmol runtime. |
| `core` | Core library, editor shell and tests only — no Axmol, no window. Builds in seconds. |
| `asan` | `core` + AddressSanitizer/UBSan, for hunting memory bugs. |

Each preset builds into its own folder under `build/`. Downloaded Axmol lives
in `external/` and is shared by all presets. To start completely fresh,
delete `build/` (and `external/` to re-download Axmol).

## Linux (Ubuntu 24.04)

```sh
sudo apt install build-essential cmake ninja-build git pkg-config \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libfontconfig1-dev \
    libgtk-3-dev libwebkit2gtk-4.1-dev libwayland-dev libxkbcommon-dev
sudo snap install powershell --classic     # or see Microsoft's install guide
cmake --workflow --preset dev
./build/dev/bin/lantern_player/lantern_player
```

On a machine without a screen (CI), run the tests under a virtual display:
`xvfb-run -a ctest --preset dev`.

## Windows

Not yet verified. Planned via CI (`.github/workflows/ci.yml`). Expected, from
a "Developer PowerShell for VS 2022":

```powershell
cmake --workflow --preset dev
```

## Troubleshooting

| Message | Fix |
|---|---|
| `PowerShell 7 ('pwsh') was not found` | Install it (see step 1), open a new Terminal, configure again. |
| `external/axmol is not at the pinned commit` | Delete the `external/axmol` folder and configure again. |
| `Could not download Axmol` | Check your internet connection; configure again. |
