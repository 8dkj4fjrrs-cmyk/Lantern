# Sample game

A small example of Lantern gameplay: walk around a meadow, pick up gems and
go through a door into a cave.

- `src/` — the game module: four behaviours (`PlayerController`,
  `SceneDoor`, `Collectible`, `Spinner`) and the module registration
  (`SampleGame.cpp`). Read [docs/gameplay.md](../../docs/gameplay.md) first.
- `project/` — the Lantern project: two scenes, a tileset and a hero.
- `tools/` — the Python scripts (Python 3 with Pillow; not needed to build or
  play) that drew the art (`make_art.py`) and wrote
  the first version of the project files (`make_project.py`). Open the
  project in the editor to change it; the scripts are only a record.

The module is built into the editor and the player by default
(`LANTERN_BUILD_SAMPLE_GAME=ON`).

**Play it** (from the repository folder, after building):

```sh
# macOS
./build/dev/bin/lantern_player/lantern_player.app/Contents/MacOS/lantern_player --project examples/sample_game/project
# Linux / Windows
./build/dev/bin/lantern_player/lantern_player --project examples/sample_game/project
```

Arrow keys or W A S D walk; Escape quits. Or open the project in the editor
(**File > Open Project…**) and press ▶.

All art here is original and, like the rest of Lantern, MIT-licensed.
