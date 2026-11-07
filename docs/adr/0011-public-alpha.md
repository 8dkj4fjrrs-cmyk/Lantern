# ADR 0011: Public alpha — templates, exporting, packaging, licenses (Milestone 8)

- **Status:** Accepted; decision 6's handling of yasio is replaced by
  [ADR 0012](0012-no-yasio.md) (yasio and Axmol's networking removed)
- **Date:** 2026-11-03
- **Milestone:** 8

## Context

The brief's last milestone is a public alpha:
- a project creation wizard;
- a usable packaging and export workflow;
- clean builds verified on supported platforms;
- better error messages and onboarding;
- license notices, documentation, example projects and release
  instructions;
- an honest list of known limitations.

The brief's goal is that someone can:
1. download Rolo;
2. create a project in the editor;
3. build a small game;
4. export a playable application.

Two facts shaped the work:
- gameplay so far needed C++ (even walking was a game module's behaviour),
  so a downloaded editor could not make a game that moves;
- the only system we can build and run on is Linux.

## Decisions

### 1. Built-in behaviours make code-free games possible

`rolo_gameplay` gains `TopDownController`, `SceneDoor` and
`CameraBounds`. These are general versions of what The Lantern Keeper's
module had. `Game::changeScene(scene, arriveAt)` puts arrival points in the
engine: the player is placed at the named object when the new scene
starts, instead of in a game variable. They are ordinary behaviours, so
games can still replace them. Keeping them in the engine library keeps the
rule that game code is C++ in behaviours, while giving every editor (built
or downloaded) a working top-down game.

### 2. Templates are projects; the wizard copies them

A template is a folder with `template.json` (name, description, order) and
a `project/` copied as is; the new project is then renamed. The example
games with a `template.json` are offered the same way.

- Shipped: **Empty**, **Top-Down Starter** (built from a fresh project by a
  script, using only built-in components) and The Lantern Keeper.
- Copying keeps the template's asset and object IDs. That is fine, because
  IDs only need to be unique within a project.
- `--new-project FOLDER --template ID` does the same from the command line.

### 3. The editor finds its own files from where it is

`RoloResources` locates templates, examples, licenses, docs and the game
runtime:
- beside the editor (an installed folder);
- in the macOS app's Resources;
- else in the source tree and build folder (a development build).

The program's own path comes from `executablePath()` in `rolo_core`,
the one place with per-OS code for it. Tests cover all three layouts with
stand-in folders.

### 4. An export is the runtime plus the project, found by the runtime itself

`exportGame` (editor model, unit-tested for each OS's layout with a stand-in
runtime) copies three things:
- the runtime's folder, with the program renamed after the game (on macOS,
  the `.app` with its Info.plist renamed);
- the project into `game/`;
- `licenses/`.

The player plays the bundled `game/` when started with no arguments. That
means:
- no launcher script or configuration file;
- the project format is unchanged;
- the same runtime binary serves the editor's tests, exports and
  developers.

Before copying, the editor checks the whole project (scenes with their
components and references, content, start scene). Errors stop the export,
because a game that fails on a player's machine is worse than an export that
explains itself. The export never deletes a folder that is not an earlier
export (a marker file says so). Exports are per host system: cross-export
would need every system's runtime in each download.

### 5. One install layout, used by the package and the editor alike

`cmake/RoloInstall.cmake` installs only a `rolo` component: Axmol's
bundled libraries come with install rules of their own. `rolo_package`
installs into `dist/<name>/` and zips it, with `cmake -E tar` so it works the
same everywhere. The layout is exactly what `RoloResources` looks for, so
an installed editor finds its templates and runtime without configuration.
The release preset and `.github/workflows/release.yml` build, test and
package on all three systems from a tag.

### 6. License texts travel with every game

`licenses/` holds the full texts of everything compiled into the runtime,
fetched by `tools/release/fetch_licenses.py` from each project at the
version Rolo uses. The list comes from the build's link line and from
the third-party headers its sources include (`ninja -t deps`). That second
source found six header-only libraries the notices had missed. Exports and
installs copy the folder.

yasio's upstream license adds the "Anti 996" terms to MIT: you must display
the license and comply with labour laws. It is kept, and disclosed in the
notices, the known limitations and every game's `licenses/`. Removing it
would mean changing Axmol's core, which includes yasio and OpenSSL
unconditionally. Websocket-parser's license turned out to be BSD-3-Clause
upstream, not MIT.

### 7. Onboarding is part of the product

The onboarding pieces:
- a welcome window on start without a project (new project, try the
  example, open);
- a folder browser in Rolo UI (Browse… in New Project, Open Project and
  Export), so nobody types paths;
- Help menu entries for the guides.

The documentation: getting started, exporting, releasing, known
limitations, CONTRIBUTING and CHANGELOG. Messages say what to do, not only
what failed, for example: a missing runtime ("reinstall Rolo, or build
rolo_player"), a missing arrival point, an export into the project
folder.

### 8. Platform support is stated as tested, not as written

Only Linux was built and tested. The macOS and Windows code paths exist
(bundles, DLLs, executable path, user folders) and CI is configured, but
neither has run. The known limitations say so in a table, and the version
is `0.8.0-alpha`.

## Consequences

- No file format changes. The project format and every content format stay
  at version 1.
- The version gains a suffix (`ROLO_VERSION_SUFFIX`, shown as
  `0.8.0-alpha`).
- New command-line options: `--template`, `--export` (editor) and `--stats`
  (player).
- New editor dependency on `nlohmann/json` (already used by the engine) for
  `template.json`.
- Open for after the alpha:
  - running CI on GitHub and fixing what macOS and Windows reveal;
  - code signing;
  - custom C++ without building Rolo from source;
  - sound;
  - removing Axmol's unused networking libraries.
