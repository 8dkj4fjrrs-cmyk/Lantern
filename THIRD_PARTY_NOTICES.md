# Third-Party Notices

Lantern Engine's own code is MIT-licensed (see `LICENSE`). This file lists
third-party software it uses, what for, and whether it ends up inside games
built with Lantern ("shipped").

Third-party source is never copied into this repository: Axmol is downloaded
by CMake into `external/` at a pinned commit (see `cmake/LanternAxmol.cmake`).

## Runtime backend

| Component | Version | License | Shipped? |
|---|---|---|---|
| [Axmol Engine](https://github.com/axmolengine/axmol) | 2.11.5 (`6c0dce6`) | MIT | Yes |
| [axslcc](https://github.com/axmolengine/axslcc) shader compiler | 1.14.0 | see upstream | No — build tool only |
| [Dear ImGui](https://github.com/ocornut/imgui) (docking branch, bundled as Axmol's ImGui extension, incl. `imgui_freetype` and `imgui_stdlib`) | 1.92.1 | MIT | Yes — editor UI; also linked into the player (unused there) |

### Libraries bundled by Axmol and linked into `lantern_player`

Taken from the Linux link line of the Milestone 1 build and Axmol's
`3rdparty/README.md`. The macOS set is similar but uses Apple's Metal instead
of Glad/GLFW-on-X11; it will be re-checked from a macOS build.

| Library | Version | License |
|---|---|---|
| astcenc | 5.3.0 | Apache-2.0 |
| Clipper2 | 2.0.1 | BSL-1.0 |
| ConvertUTF | git 20451cb | Apache-2.0 WITH LLVM-exception |
| curl | 8.17.0 | curl (MIT/X-style) |
| FastLZ | git 344eb40 | MIT |
| {fmt} | 11.2.0 | MIT |
| FreeType | 2.14.1 | FreeType License (BSD-style) |
| Glad | 2.0.8 | MIT |
| GLFW (modified by Axmol) | 3.4 | zlib |
| libjpeg-turbo | 3.1.3 | IJG / BSD-3-Clause / zlib |
| llhttp | 9.3.0 | MIT |
| OpenSSL | 3.0.18 | Apache-2.0 |
| libpng | 1.6.53 | PNG Reference Library License v2 |
| poly2tri | git 81612cb | BSD-3-Clause |
| pugixml | 1.13 | MIT |
| simdjson | 3.10.1 | Apache-2.0 |
| minizip (unzip) | 1.2 (modified) | zlib |
| libwebp | 1.6.0 | BSD-3-Clause (Google) |
| websocket-parser | (bundled) | MIT upstream — no license file in Axmol's copy; to verify |
| xxHash | 0.8.3 | BSD-2-Clause |
| yasio | 4.3.2 | MIT WITH Anti-996 — **review before public release** |
| zlib | 1.3.1 | zlib |

**Not included** in Milestone 1 builds: OpenAL Soft (LGPL-2.1), because
Axmol audio is switched off. It must be dynamically linked or replaced when
audio is enabled.

**Open items before a public release**
- yasio's "Anti-996" addition is not a standard OSI license term; confirm it
  is acceptable, or disable the Axmol networking features that pull it in.
- curl, OpenSSL, llhttp, websocket-parser and yasio are networking libraries
  Lantern does not use yet; investigate switching them off to shrink games
  and the notice list.
- Shipped games must include these notices; the packaging step (Milestone 8)
  will generate them automatically.

## Fonts

The editor loads a font already installed on the user's computer (San
Francisco / Arial on macOS, Segoe UI / Arial on Windows, DejaVu Sans /
Liberation Sans on Linux) at run time. No font files are bundled or
redistributed; otherwise Dear ImGui's built-in font (ProggyClean, MIT-style,
part of Dear ImGui) is used.

## Development and test tools (not shipped)

| Component | Version | License | Used for |
|---|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | v3.16.1 | BSL-1.0 | Unit tests |
| [PowerShell](https://github.com/PowerShell/PowerShell) | 7.x | MIT | Axmol's download helper during configure |

## Planned

| Component | License | Notes |
|---|---|---|
| nlohmann/json | MIT | Project, scene and content files (Milestone 3). |
