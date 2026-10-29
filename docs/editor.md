# Lantern Editor guide

> Milestone 2: the editor shows the running scene. Creating and editing
> scenes arrives in Milestone 3.

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
| File  View  Help                                                 |
|              [Start Scene v] [>] [@]                  [Build] [F]|  main toolbar
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
| **Hierarchy** | Every object in the running scene as a tree. Click to select. Type in **Search name** to show only matching objects (and the objects they sit inside). |
| **Assets** | Your project's files. Lantern projects arrive in Milestone 3; until then it says "No project open". |
| **Scene** | The game world as players will see it, scaled to fit. The strip above it shows the game's resolution and zoom; the move, rotate, scale and rect tools arrive with scene editing. |
| **Assets Preview** | Thumbnails of the folder selected in Assets (Milestone 3). |
| **Console** | Messages from the engine, the runtime and the editor. Filter by level or text, copy, clear. A message longer than one line shows "+N lines"; **Copy** gives the full text. |
| **Inspector** | The selected object's properties in folding sections (Node, Rendering, Hierarchy), live and read-only for now. **(?)** explains a section. |

The main toolbar's scene selector, **Play**, **Reload**, **Build** and
project-folder buttons, the panels' **+** buttons and **Add Component** are
in place but greyed out: hover over one to see which milestone brings it.

The status bar shows the frame rate, how many messages, warnings and errors
the Console holds, and the Lantern version.

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
`editor.prefs` (open panels, window size). Deleting them is safe — the
defaults come back. (`layout.ini` and `layout.dock`, left by earlier preview
builds, are no longer used and can be deleted.)

## Command-line options

```
--help              Show all options
--version           Print version information
--reset-layout      Start with the default panel layout
--config-dir DIR    Keep settings in DIR instead (useful for testing)
--frames N          Quit after N frames (used by automated tests)
--screenshot FILE   Save the last frame as PNG (needs --frames)
```
