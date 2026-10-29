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

```
+-----------+-----------------------------+-----------+
| Hierarchy |          Viewport           | Inspector |
|           +-----------------------------+           |
|           |          Console            |           |
+-----------+-----------------------------+-----------+
| status bar: version, backend, fps, errors/warnings  |
+-----------------------------------------------------+
```

| Panel | What it does |
|---|---|
| **Viewport** | The game world as players will see it, scaled to fit. The caption shows the game's resolution and the zoom. |
| **Hierarchy** | Every object in the running scene as a tree. Click to select; click empty space to clear. |
| **Inspector** | The selected object's position, rotation, scale, size, draw order and more — live, read-only for now. |
| **Console** | Messages from the engine, the Axmol backend and the editor. Filter by level or text, copy, clear. Hover a "(more...)" message to read all of it. |

### Arranging panels

- Drag a panel's tab to move it; drop it on the arrows that appear to dock it
  beside, above or below another panel, or in the middle to make tabs.
- Drag the borders between panels to resize.
- Close a panel with its **x**; bring it back from the **View** menu.
- **View > Reset Layout** restores the default arrangement.

The layout and which panels are open are remembered between sessions.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| Cmd+Q (macOS) / Ctrl+Q | Quit |

## Where settings are kept

| OS | Folder |
|---|---|
| macOS | `~/Library/Application Support/Lantern Engine/Editor` |
| Windows | `%APPDATA%\Lantern Engine\Editor` |
| Linux | `~/.config/lantern-engine/editor` (or `$XDG_CONFIG_HOME/lantern-engine/editor`) |

Two files live there: `layout.ini` (panel arrangement) and `editor.prefs`
(open panels, window size). Deleting them is safe — the defaults come back.

## Command-line options

```
--help              Show all options
--version           Print version information
--reset-layout      Start with the default panel layout
--config-dir DIR    Keep settings in DIR instead (useful for testing)
--frames N          Quit after N frames (used by automated tests)
--screenshot FILE   Save the last frame as PNG (needs --frames)
```
