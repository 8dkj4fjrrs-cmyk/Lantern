# Known limitations (0.8.0-alpha)

Lantern is an alpha: it works end to end, from a new project to an
exported game, but much is missing or untested. This page lists what we
know. Please report anything else.

## What has been tested where

| | Linux x86_64 | macOS Apple silicon | Windows x64 |
|---|---|---|---|
| Builds (development and release) | **Yes**, every change | Not yet: the code and CI job exist, but no build has run | Not yet: same |
| Automated tests (unit + real windows) | **Yes**, every change | Not yet | Not yet |
| Editor used by hand | **Yes**, on a virtual display | No | No |
| Exported games run | **Yes** (export, move elsewhere, start with no arguments) | No | No |

Lantern was planned for macOS first. The macOS paths are written:
- app bundles, Metal through Axmol;
- the program's own location (`_NSGetExecutablePath`);
- user folders, and exporting `.app` bundles with their Info.plist.

The Windows paths are written too: `.exe`, the DLLs beside it, and user
folders. So far, though, the only real builds have been on Linux.
`.github/workflows/ci.yml` builds and tests on all three systems as soon as
the repository is on GitHub. **Until it has run, treat macOS and Windows as
untested.**

## Exporting

- Games are exported for the system the editor runs on; there is no
  cross-export.
- macOS apps are **not signed or notarized**: players must right-click
  **Open** the first time.
- No installer: games are folders (or `.app` bundles) to zip and share.
- Every game contains the whole runtime, including networking libraries
  that Axmol builds in and Lantern never uses (curl, OpenSSL, yasio and
  others). They add size and license notices, not behaviour.
- **yasio's license adds the "Anti 996" terms** to MIT. You must comply with
  labour laws, and you must display the license, which exports do by
  including `licenses/`. These terms are not a standard open-source license;
  see [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Removing yasio
  would mean changing Axmol.

## Games

- **No sound**: Axmol's audio is switched off (its OpenAL Soft is LGPL and
  needs careful linking). Sound files can be imported, but nothing plays them.
- **Keyboard only**: no gamepads or touch. The mouse position is available
  to C++ behaviours.
- The window is 1280 × 720 points. There is no fullscreen
  setting, title screen, pause menu or options menu. Escape quits at once,
  and **F5 / F9 save and load** in every game.
- Collision uses axis-aligned boxes and tiles; there is no physics engine.
  Rotated colliders use the box around them.
- No lighting: the night in The Lantern Keeper is a dark overlay.
- Text is one font size per Text object. There is no localisation.
- Desktop only: no mobile or web.

## Editor

- **Your own C++ behaviours need Lantern built from source.** Game modules
  are linked into the editor and the player when they are built (see the
  [gameplay guide](gameplay.md)). A downloaded Lantern has:
  - the built-in components;
  - Top-Down Controller, Scene Door and Camera Bounds;
  - the modules of its two example games.

  This is the main thing to change after the alpha.
- Scenes cannot be renamed, and folders cannot be created, in the editor.
  Use your file manager; the editor notices.
- One object can be selected at a time. There are no prefabs (reusable
  objects); copy and paste or duplicate instead.
- Dialogue is edited as forms in the Inspector, with one-line text fields.
  There is no visual dialogue graph.
- No timeline animation: flipbook clips only (see the [roadmap](roadmap.md)).
- Lantern has its own folder browser, not your system's file dialog.
- Changes to anything but positions rebuild the drawn scene; fine for
  scenes of hundreds of objects, not tested with thousands.

## Files

- Formats are versioned and documented ([formats](formats/scene.md)). Until
  1.0 a format may still change. Lantern will upgrade older files when it
  does, as it promises in each format's description.
- Saves made with one version of a game keep working after the game changes
  (see [content formats](formats/content.md#save-games-lsave)). Saves are
  not encrypted or protected.
