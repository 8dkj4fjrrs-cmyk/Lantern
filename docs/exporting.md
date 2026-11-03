# Exporting games

Exporting makes a copy of your game that runs **without the editor**, to give
to players. It contains:
- Lantern's game runtime (the player program, renamed after your game);
- your project's files;
- the license texts of the software inside it.

## In the editor

Click **Build** in the toolbar, or **File > Export Game…**:

| Field | Meaning |
|---|---|
| Game name | Names the game's folder (or macOS app) and its program. Letters, digits, spaces, `-` and `_` are kept. |
| Export into | The folder the game is made in (by default `Documents/Lantern Exports`). It cannot be inside the project. |

If the open scene has unsaved changes, it is saved first. **Exporting again**
replaces the earlier export of the same game. A folder of the same name
that is *not* an export is never touched.

### Checks before exporting

Lantern first reads everything the game needs, as the player program would.
That is:
- the project file and its start scene;
- every scene, with its components, the assets it uses, and its triggers'
  conditions and actions;
- the story content: variables, dialogues and quests.

**Errors stop the export.** For example, a missing start scene, a scene
using an unknown behaviour, a dialogue leading to a line that does not exist,
or a trigger naming an object that is not in its scene. The Console lists
each one with its file and place. **Warnings** are reported but do not stop
it. **Game > Check Content** runs most of the same checks at any time.

## From the command line

```sh
lantern_editor --project "path/to/My Game" --export "path/to/Exports"
```

This runs the same checks and export without opening a window. It prints
the game program's path and exits with code 0, or 1 if it failed. It is
useful in scripts and continuous integration.

## What you get

```
Linux and Windows                    macOS
My Game/                             My Game.app/
  My Game           (My Game.exe)      Contents/MacOS/lantern_player
  axslc/            shaders            Contents/Resources/axslc/
  game/             your project       Contents/Resources/game/
  licenses/         license texts      Contents/Resources/licenses/
  README.txt        how to play        Contents/Info.plist   (your game's name)
```

To share the game, **zip the whole folder** (or the `.app`), not only the
program, and **keep the `licenses` folder with it**. The licenses of
Lantern and the libraries in it require that their texts go with every copy;
see [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) and
[licenses/](../licenses/README.md).

Players run the program. When the runtime starts with no arguments, it looks
for `game/` beside itself (on macOS, in the app's Resources) and plays it:
- the window is titled with your game's name;
- the frame-rate overlay is hidden (`--stats` shows it);
- saved games go to the player's own user folder, per game.

## Each system exports its own games

An export contains the runtime of the computer it was made on, so:
- export on macOS to make the macOS version;
- export on Windows for Windows;
- export on Linux for Linux.

There is no cross-export yet.

- **macOS**: the app is not signed or notarized by Apple. The first time,
  players right-click it and choose **Open**. Signing needs an Apple
  Developer account; it is on the roadmap. Exported apps run on the same kind
  of Mac they were made on (Apple silicon from an Apple silicon build).
- **Windows**: the game is a folder with `My Game.exe` and the DLLs it needs.
- **Linux**: the game is a folder; the program needs a desktop with OpenGL,
  as the editor does.

The [known limitations](known-limitations.md) say which of these have been
tested.

## Size

A game made from a **release build** of Lantern (or a downloaded Lantern)
is the runtime plus your files: the Top-Down Starter exports to about 16 MB
on Linux, most of it the rendering engine and its libraries. Zipped, it is
smaller.

The editor of a *development* build (`--preset dev`) exports its debug
runtime, which is much larger (about 90 MB), and slower. Use a release
build, or a downloaded Lantern, for games you share.
