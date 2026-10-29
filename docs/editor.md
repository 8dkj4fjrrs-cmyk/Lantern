# Lantern Editor guide

> Milestone 3: projects and scenes you can create, edit (with undo) and
> save. Sprites, tilemaps, the camera and dragging objects in the Scene view
> arrive in Milestone 4.

## Starting the editor

macOS:

```sh
./build/dev/bin/lantern_editor/lantern_editor.app/Contents/MacOS/lantern_editor
```

(or `open build/dev/bin/lantern_editor/lantern_editor.app`, without log output)

Linux / Windows: `build/dev/bin/lantern_editor/lantern_editor`

## The workspace

The editor uses the classic game-editor arrangement:

```
+------------------------------------------------------------------+
| File  Edit  View  Help                                           |
| [save][undo][redo]    [main v] [>] [revert]          [Build] [F] |  main toolbar
+-----------+-------------------------------------+----------------+
| Hierarchy |  Scene                              |  Inspector     |
|           |                                     |                |
+-----------+                                     |                |
| Assets    +-------------------------------------+                |
|           |  Assets Preview | Console          |                |
+-----------+-------------------------------------+----------------+
| fps                              (i) 12  (!) 0  (x) 0  Version   |  status bar
+------------------------------------------------------------------+
```

| Panel | What it does |
|---|---|
| **Hierarchy** | The scene's objects as a tree; the top row is the scene itself. Click to select. **+** creates an object (Empty, Rectangle, Ellipse, or one inside the selected object). Type in **Search name** to show only matching objects. Delete or Backspace deletes the selected object. |
| **Assets** | The project's files. Double-click a scene to open it; **+** creates a new scene. Changes made outside the editor show up within a few seconds. |
| **Scene** | The game as players will see it, scaled to fit. Click an object to select it; the selected object gets an outline. The strip above shows the game's resolution and zoom. The move, rotate, scale and rect tools arrive in Milestone 4. |
| **Assets Preview** | Thumbnails of images and other assets (Milestone 4, with asset import). |
| **Console** | Messages from the engine, the runtime and the editor, including any problems found when opening a scene. Filter by level or text, copy, clear. |
| **Inspector** | The selected object: its on/off switch and name, then one folding section per component with fields you can edit. **Add Component** adds a component; a section's **⋮** menu removes or resets it; **(?)** explains it. With the scene row selected it shows the scene's name and file. |

The toolbar's **Play** and **Build** buttons are greyed out: hover over one to
see which milestone brings it.

The status bar shows the frame rate, how many messages, warnings and errors
the Console holds, and the Lantern version.

## Projects and scenes

A project is a folder holding your game: a `project.lantern` settings file
and a `scenes` folder (see [file formats](formats/scene.md)).

- **File > New Project…** asks for a name and a location (by default
  `Documents/Lantern Projects`) and creates the folder with a first scene.
- **File > Open Project…** lists recently opened projects, or takes a
  project folder's path. The editor reopens the last project at start.
- Without a project, the editor shows the start scene; you can edit it and
  save it into a new project.
- **File > New Scene**, **Save Scene**, **Save Scene As…** (also how you
  duplicate a scene) and **Revert to Saved**. The scene selector in the
  toolbar switches between the project's scenes.

A "•" after the scene's name (in the toolbar and the window title) means it
has unsaved changes. If you quit, close the window or open another scene
with unsaved changes, the editor asks whether to save them first.

## Editing

Every change can be undone: **Edit > Undo** (⌘Z) and **Redo** (⇧⌘Z). The
Edit menu names what will be undone, e.g. "Undo Rename". The undo history
belongs to the open scene; opening another scene starts a new one.

| To | Do |
|---|---|
| Create an object | Hierarchy **+**, or **Edit > New Rectangle** (and others) |
| Rename it | Edit the name at the top of the Inspector, or press F2 |
| Duplicate it (with its children) | ⌘D |
| Delete it | Delete / Backspace in the Hierarchy, or **Edit > Delete** |
| Change its drawing order | **Edit > Move Up / Move Down** (later siblings are drawn on top) |
| Hide it | Untick the box next to its name in the Inspector |
| Change a number | Type it and press Enter (Esc cancels), use ↑ ↓ to nudge, or drag the small label at the field's right end (X, Y, °) left and right |
| Change a colour | Type a hex value (`#ffb84d`), or click the swatch to pick one |

Positions are in points from the parent's position, with y pointing up.
Rotation is in degrees, clockwise.

### Arranging panels

- Drag a panel's tab to move it. A highlighted area shows where it will land:
  near the edge of another panel to put it beside, above or below that panel,
  or in the middle to add it as a tab there.
- Drag the borders between panels to resize.
- Close a panel with the **x** that appears when you hover over its tab, or
  from the panel's **☰** menu (top right of each panel); bring it back from
  the **View** menu.
- **View > Reset Layout** restores the default arrangement.

The layout and which panels are open are remembered between sessions.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| ⌘S / ⇧⌘S | Save scene / Save scene as |
| ⌘N | New scene |
| ⌘Z / ⇧⌘Z (Ctrl+Z / Ctrl+Y) | Undo / Redo |
| ⌘D | Duplicate the selected object |
| F2 | Rename the selected object |
| Delete or Backspace (in the Hierarchy) | Delete the selected object |
| ⌘Q (macOS) / Ctrl+Q | Quit |
| Tab / Shift+Tab | Move keyboard focus between controls |
| ↑ ↓ ← → | Move through the Hierarchy (← → fold and unfold) and the Console |
| ⌘A, ⌘C, ⌘X, ⌘V (Ctrl on Windows/Linux) | Select all, copy, cut, paste in text fields |
| ⌥← ⌥→ (Ctrl on Windows/Linux) | Move by word in text fields |
| Esc | Close a menu or dialog |

## Where settings are kept

| OS | Folder |
|---|---|
| macOS | `~/Library/Application Support/Lantern Engine/Editor` |
| Windows | `%APPDATA%\Lantern Engine\Editor` |
| Linux | `~/.config/lantern-engine/editor` (or `$XDG_CONFIG_HOME/lantern-engine/editor`) |

Two files live there: `panels.dock` (panel arrangement, one line of text) and
`editor.prefs` (open panels, window size, recent projects). Deleting them is safe — the
defaults come back. (`layout.ini` and `layout.dock`, left by earlier preview
builds, are no longer used and can be deleted.)

On Windows and Linux, read Ctrl for ⌘ and Alt for ⌥.

## Command-line options

```
--help                Show all options
--version             Print version information
--project FOLDER      Open the Lantern project in FOLDER
--new-project FOLDER  Create a new project in FOLDER and open it
--no-project          Start without opening the last project
--reset-layout        Start with the default panel layout
--config-dir DIR      Keep settings in DIR instead (useful for testing)
--frames N            Quit after N frames (used by automated tests)
--screenshot FILE     Save the last frame as PNG (needs --frames)
```
