# Third-Party Notices

Lantern Engine's own code is MIT-licensed (see `LICENSE`). This file lists
third-party software it uses, what for, and whether it ends up inside games
built with Lantern ("shipped").

Large dependencies are not copied into this repository: Axmol is downloaded
by CMake into `external/` at a pinned commit (see `cmake/LanternAxmol.cmake`).
Two small ones are kept in the repository, unmodified and with their license
files: `third_party/stb/stb_truetype.h`, `third_party/stb/stb_image.h` and the
Inter fonts in `assets/fonts/`.

## Runtime backend

| Component | Version | License | Shipped? |
|---|---|---|---|
| [Axmol Engine](https://github.com/axmolengine/axmol) | 2.11.5 (`6c0dce6`) | MIT | Yes |
| [axslcc](https://github.com/axmolengine/axslcc) shader compiler | 1.14.0 | see upstream | No — build tool only |

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
| stb_image (Axmol's copy) | 2.30 | MIT or public domain |
| minizip (unzip) | 1.2 (modified) | zlib |
| libwebp | 1.6.0 | BSD-3-Clause (Google) |
| websocket-parser | (bundled) | MIT upstream — no license file in Axmol's copy; to verify |
| xxHash | 0.8.3 | BSD-2-Clause |
| yasio | 4.3.2 | MIT WITH Anti-996 — **review before public release** |
| zlib | 1.3.1 | zlib |

**Not included** in Lantern builds: Axmol's optional extensions (its Dear ImGui
integration, inspector, SDF generator, and so on) are switched off.
**Not included** either: OpenAL Soft (LGPL-2.1), because
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

## Lantern UI (the editor's interface toolkit, `engine/ui`)

Lantern's UI toolkit is Lantern's own code. It uses:

| Component | Version | License | Shipped? |
|---|---|---|---|
| [stb_truetype](https://github.com/nothings/stb) (`third_party/stb/stb_truetype.h`) | 1.26 (`003501f6`) | MIT or public domain (Unlicense), at our choice: MIT | Yes — in the editor; in games only once they use Lantern UI |
| [Inter](https://rsms.me/inter/) Regular and SemiBold (`assets/fonts/`) | 4.0 (font version 4.001) | SIL Open Font License 1.1 (`assets/fonts/Inter-LICENSE.txt`) | Yes — embedded in the program file |

The fonts are embedded unmodified. The OFL allows bundling and embedding them
in software, including commercial software, as long as the license text goes
with them and the fonts are not sold on their own. "Inter" is a Reserved Font
Name only if the font itself is modified — we do not modify it.

## Development and test tools (not shipped)

| Component | Version | License | Used for |
|---|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | v3.16.1 | BSL-1.0 | Unit tests |
| [PowerShell](https://github.com/PowerShell/PowerShell) | 7.x | MIT | Axmol's download helper during configure |

## Scenes and project files (`engine/scene`)

| Component | Version | License | Shipped? |
|---|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 (release archive, SHA-256 checked) | MIT | Yes: reads and writes project and scene files in the editor and in games |

Downloaded by CMake at configure time (`engine/scene/CMakeLists.txt`), not
copied into this repository. It is a header-only library; its MIT notice
must accompany shipped games (handled by the packaging step, Milestone 8).

## Assets (`engine/assets`)

| Component | Version | License | Shipped? |
|---|---|---|---|
| [stb_image](https://github.com/nothings/stb) (`third_party/stb/stb_image.h`) | 2.30 (`003501f6`, the same stb commit as stb_truetype) | MIT or public domain (Unlicense), at our choice: MIT | Yes: checks image files (size, validity) in the editor and in games |

Compiled into `AssetDatabase.cpp` only, with every function `static` and
only the PNG and JPEG decoders, because Axmol carries its own copy of
stb_image for loading textures (listed with Axmol's libraries). The two
copies never meet at link time.
