# Getting started with Lantern

This guide goes from nothing to a small game you can send to a friend, in
about half an hour. No programming is needed. Lantern is an **alpha**: read
the [known limitations](known-limitations.md) first.

## 1. Get Lantern

- **A download** (when releases are published): unzip
  `Lantern-<version>-<system>.zip` anywhere and run `lantern_editor` (on
  macOS, `lantern_editor.app`; the first time, right-click it and choose
  **Open**, because the alpha is not signed by Apple).
- **From source**: follow [building](building.md), then run
  `build/dev/bin/lantern_editor/lantern_editor` (on macOS,
  `build/dev/bin/lantern_editor/lantern_editor.app`).

## 2. Make a project

The first time, the editor shows **Welcome to Lantern**. Click **New
Project…** (later: **File > New Project…**).

- **Start from**: choose **Top-Down Starter**. It is a small game that
  already works: a meadow and a grove, a hero who walks, a friend to talk to
  and a first quest. (**Empty Project** starts with nothing;
  **Example: The Lantern Keeper** is a finished game to study.)
- **Project name**: e.g. *My First Game*.
- **Location**: where its folder goes (by default
  `Documents/Lantern Projects`; **Browse…** to choose another).

Click **Create Project**. The editor opens the project's first scene. Its
parts:
- the scene's objects are listed in the **Hierarchy** (left);
- the project's files are in **Assets** (below it);
- the selected object's settings are in the **Inspector** (right).

## 3. Play it

Press **▶** in the toolbar (or ⌘P / Ctrl+P):
1. Walk with the arrow keys or W A S D.
2. Walk up to the friend and press **E** to talk; arrows choose answers.
3. Walk off the right edge of the meadow, along the path, to reach the grove.

Press **■** to stop. Playing never changes your scene: the game plays a copy.

## 4. Make it yours

- **Paint the ground.** Select **Ground** in the Hierarchy, pick a tile in the
  **Tile Palette** (bottom), and paint in the Scene view. B is the brush,
  X the eraser and G fill.
  Water, trees and walls block the hero because their tiles are marked solid
  (**Edit solid tiles** in the Tile Palette).
- **Move things.** Drag the Friend or the Signpost in the Scene view (the
  move tool, W).
- **Change what people say.** Select `content/friend.ldialogue` in Assets
  and edit the lines in the Inspector. Choices, and lines that only show
  when something is true, work the same way (see the
  [editor guide](editor.md#story-content-dialogues-quests-variables)).
- **Add someone.** Select Friend and duplicate it (⌘D / Ctrl+D). Move the
  copy, then make a new dialogue (**Game > New Dialogue**). Choose it in the
  copy's Talk Area: its Trigger's action is `startDialogue <name>`.
- **Use your own art.** Drag PNG files onto the editor window. For a
  character, select the sheet and click **Create Animations…**, then make
  `walk_down`, `idle_down`… clips in the **Animation** panel. The Top-Down
  Controller plays them by those names.

Every change can be undone (⌘Z / Ctrl+Z). Save the scene with ⌘S / Ctrl+S.

## 5. Share it

Click **Build** in the toolbar (or **File > Export Game…**). Choose the
game's name and a folder, then **Export**.

Lantern checks the project, then makes a folder holding the game's program,
its files, and the license texts that must go with it. Zip that folder and
send it: your friend runs the program inside, with no editor needed. Each
export runs on the kind of computer it was made on (macOS, Windows or
Linux); see [exporting](exporting.md).

## Where next

- [Editor guide](editor.md): every panel and tool.
- [Making The Lantern Keeper](making-the-lantern-keeper.md): a whole RPG,
  step by step.
- [Gameplay guide](gameplay.md): triggers, quests and saving without code,
  and C++ behaviours when you want more.
- [File formats](formats/scene.md): what Lantern writes, for version control.
